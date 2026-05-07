// ====================================================================
//  motor_ctrl.cpp — Logika upravljanja koračnim motorom
//
//  StepperRMT task: core 1, prioritet 10
//  Homing task:     core 0, prioritet 5  (pokreće se privremeno)
//  Status/main:     core 0, prioritet 1  (Arduino loop)
// ====================================================================
#include "motor_ctrl.h"
#include <StepperRMT.h>
#include <Arduino.h>

// ── Default konfiguracija (precišćuje se prvim CMD_SYNC od HMI) ──────────
MotorConfig g_motor_cfg = {
    .steps_per_mm        = 640.0f,
    .max_travel_mm       = 500.0f,
    .home_speed_mmps     = 5.0f,
    .auto_speed_mmps     = 30.0f,
    .accel_mmps2         = 150.0f,
    .home_offset_mm      = 0.0f,
    .home_fast_speed_mmps= 20.0f,
    .home_odmak_mm       = 5.0f,
    .home_clearance_mm   = 5.0f,
    .overshoot_mm        = 2.0f,
    .approach_speed_mmps = 5.0f,
    .retract_mm          = 2.0f,
    .retract_speed_mmps  = 20.0f,
    .direction           = 1,
};

// ── StepperRMT objekat ────────────────────────────────────────────────────
// 3200 coraka/okretaju (200 base × 16 mikrostep)
static StepperRMT stepper(PIN_PUL, PIN_DIR, PIN_ENA, 3200, RMT_CHANNEL_0);

// ── Interno stanje ────────────────────────────────────────────────────────
static volatile uint8_t s_state      = MSTATE_IDLE;
static volatile bool    s_homed      = false;
static volatile uint8_t s_alarm_code = ALARM_NONE;
static float            s_safe_max_mm     = 1000.0f; // gornji soft limit, setuje se po homingu
static bool             s_safe_max_valid  = false;   // true tek nakon prvog hominga
static bool             s_retract_enabled = true;
static bool             s_last_bend_high  = true;  // HIGH = kontakt otvoren

// ── HOME limit interrupt ──────────────────────────────────────────────────
static volatile bool s_home_irq     = false;  // postavlja ISR, briše limit_tick
static volatile bool s_drv_alarm_irq = false; // postavlja drv_alarm_isr

// NC mehanicki switch + INPUT_PULLUP: normalno LOW (zatvoren ka GND), aktivan = HIGH (otvoren)
static void IRAM_ATTR home_limit_isr(void) {
    s_home_irq = true;
    stepper.stopFromISR(); // atomski _stop flag, ~1-5ms do zaustavljanja
}

// Driver ALM interrupt: FALLING = HIGH→LOW = drajver detektovao problem
static void IRAM_ATTR drv_alarm_isr(void) {
    s_drv_alarm_irq = true;
    stepper.stopFromISR();   // atomski _stop flag, ~1-5ms do zaustavljanja
}

// ── Konverzije ────────────────────────────────────────────────────────────
// mm → steps (uzima u obzir direction)
static inline long mm_to_steps(float mm)
{
    return (long)(mm * g_motor_cfg.steps_per_mm * (float)g_motor_cfg.direction);
}

// steps → mm
static inline float steps_to_mm(long steps)
{
    return (float)steps / (g_motor_cfg.steps_per_mm * (float)g_motor_cfg.direction);
}

// mm/s → Hz (koraci/s)
static inline float speed_hz(float mmps)
{
    float spd = (mmps > 0) ? mmps : g_motor_cfg.auto_speed_mmps;
    return spd * g_motor_cfg.steps_per_mm;
}

// mm/s² → Hz/s
static inline float accel_hz(void)
{
    return g_motor_cfg.accel_mmps2 * g_motor_cfg.steps_per_mm;
}

// ── Homing task ───────────────────────────────────────────────────────────
//
//  Sekvenca (dvofazna):
//   1. Ako je senzor odmah aktivan → back 10mm
//   2. Brzo pomicanje prema senzoru (smjer = logički -mm)
//      Polling senzora svake 5ms; stepper.stop() na aktivaciju
//   3. Odmak 5mm
//   4. Sporo pomicanje prema senzoru (home_speed_mmps)
//      Polling senzora; stop na aktivaciju = precizan home
//   5. setPosition(home_offset_steps) → logička pozicija = home_offset_mm
//
static void homing_task(void *arg)
{
    (void)arg;
    // Privremeno iskljuci interrupt — homing koristi vlastitu polling petlju
    detachInterrupt(digitalPinToInterrupt(PIN_HOME));
    s_home_irq = false;
    s_state = MSTATE_HOMING;
    Serial.println("[MOTOR] Homing start");

    // Vec na senzoru? (NC HIGH = kontakti otvoreni = senzor aktivan)
    if (digitalRead(PIN_HOME) == HIGH) {
        Serial.println("[MOTOR] Sensor odmah aktivan, odmak -10mm ka komadu");
        stepper.moveRel(mm_to_steps(-10.0f),
                        speed_hz(g_motor_cfg.home_speed_mmps),
                        accel_hz());
        stepper.waitDone();
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    // ── Faza 1: brzo kretanje prema home senzoru (+mm, CCW, ka motoru) ───────
    Serial.println("[MOTOR] Faza 1: trazenje senzora (brzo, ka +mm)");
    float fast_speed = g_motor_cfg.home_fast_speed_mmps;  // Settings > HOMING BRZA
    if (fast_speed < 1.0f)   fast_speed = 10.0f;
    if (fast_speed > 400.0f) fast_speed = 400.0f;

    // Kreći se maksimalno 120% od max_travel da sigurno dođemo do senzora
    long search_steps = mm_to_steps(+(g_motor_cfg.max_travel_mm * 1.2f));
    stepper.moveRel(search_steps, speed_hz(fast_speed), accel_hz());
    vTaskDelay(pdMS_TO_TICKS(20));  // mali delay da stepper task postavi _moving=true

    bool found = false;
    while (stepper.isMoving()) {
        if (digitalRead(PIN_HOME) == HIGH) {  // NC HIGH = senzor aktivan
            stepper.softStop();   // Faza 1: meko usporenje, preciznost nije bitna
            found = true;
            Serial.println("[MOTOR] Faza 1: senzor pronadjen");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    stepper.waitDone();
    if (!found && digitalRead(PIN_HOME) == HIGH) found = true;

    if (!found) {
        Serial.println("[MOTOR] ALARM: senzor nije pronadjen!");
        s_alarm_code = ALARM_HOME_FAIL;
        s_state      = MSTATE_ALARM;
        s_home_irq   = false;
        attachInterrupt(digitalPinToInterrupt(PIN_HOME), home_limit_isr, RISING);
        vTaskDelete(nullptr);
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(150));

    // ── Faza 2: odmak -odmak ka komadu (CW, od senzora) ────────────────────
    Serial.printf("[MOTOR] Faza 2: odmak -%.1fmm ka komadu\n", g_motor_cfg.home_odmak_mm);
    stepper.moveRel(mm_to_steps(-g_motor_cfg.home_odmak_mm),
                    speed_hz(g_motor_cfg.home_speed_mmps),
                    accel_hz());
    stepper.waitDone();
    vTaskDelay(pdMS_TO_TICKS(200));

    // ── Faza 3: sporo kretanje prema senzoru (+mm, CCW, precizno) ────────
    Serial.println("[MOTOR] Faza 3: trazenje senzora (sporo, ka +mm)");
    stepper.moveRel(mm_to_steps(+20.0f),   // 20mm je dovoljno
                    speed_hz(g_motor_cfg.home_speed_mmps),
                    accel_hz() * 0.5f);    // pola akceleracije za glatkiji stop

    found = false;
    vTaskDelay(pdMS_TO_TICKS(20));  // mali delay da stepper task postavi _moving=true
    while (stepper.isMoving()) {
        if (digitalRead(PIN_HOME) == HIGH) {  // NC HIGH = senzor aktivan
            stepper.stop();
            found = true;
            Serial.println("[MOTOR] Faza 3: senzor pronadjen (finalni)");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(3));
    }
    stepper.waitDone();
    if (!found && digitalRead(PIN_HOME) == HIGH) found = true;

    if (!found) {
        Serial.println("[MOTOR] ALARM: senzor faza 3 nije pronadjen!");
        s_alarm_code = ALARM_HOME_FAIL;
        s_state      = MSTATE_ALARM;
        s_home_irq   = false;
        attachInterrupt(digitalPinToInterrupt(PIN_HOME), home_limit_isr, RISING);
        vTaskDelete(nullptr);
        return;
    }

    // ── Postavi poziciju ──────────────────────────────────────────────
    // Logička pozicija 0 je na senzoru. Ako je home_offset_mm > 0,
    // motor je "logički" na home_offset_mm poziciji kad je senzor aktivan.
    long pos_at_home = mm_to_steps(g_motor_cfg.home_offset_mm);
    stepper.setPosition(pos_at_home);

    s_homed = true;
    Serial.printf("[MOTOR] Homing ZAVRSEN. Pozicija: %.3f mm\n", motor_get_position_mm());

    // Odmakni od senzora za sigurnosni razmak (home_clearance_mm, default 5mm)
    // Motor ne smije stajati na senzoru — ovim postavljamo i novi gornji soft limit
    float clearance = (g_motor_cfg.home_clearance_mm > 0.5f) ? g_motor_cfg.home_clearance_mm : 5.0f;
    stepper.moveRel(mm_to_steps(-clearance),
                    speed_hz(g_motor_cfg.home_speed_mmps), accel_hz());
    stepper.waitDone();
    s_safe_max_mm = motor_get_position_mm();  // novi gornji limit = ova pozicija
    s_safe_max_valid = true;
    Serial.printf("[MOTOR] Odmak %.1fmm ka komadu. Gornji limit: %.3f mm\n",
                  clearance, s_safe_max_mm);

    s_state = MSTATE_IDLE;

    // Ponovo ukljuci interrupt za limit zaštitu tokom normalnog rada
    s_home_irq = false;
    attachInterrupt(digitalPinToInterrupt(PIN_HOME), home_limit_isr, RISING);
    vTaskDelete(nullptr);
}

// ── Inicijalizacija ───────────────────────────────────────────────────────
void motor_ctrl_init(void)
{
    pinMode(PIN_HOME, INPUT_PULLUP);
    stepper.begin();
    stepper.enable();
    s_state = MSTATE_IDLE;
    pinMode(PIN_BEND, INPUT_PULLUP);
    // GPIO34 je input-only, nema PULLUP u hardveru — vanjski 1kΩ pullup je obavezan
    pinMode(PIN_ALM, INPUT);
    // HOME limit interrupt: RISING = NC switch otvoren (pin LOW→HIGH = senzor aktivan)
    s_home_irq = false;
    attachInterrupt(digitalPinToInterrupt(PIN_HOME), home_limit_isr, RISING);
    // Driver ALM interrupt: FALLING = HIGH→LOW = drajver detektovao problem
    s_drv_alarm_irq = false;
    attachInterrupt(digitalPinToInterrupt(PIN_ALM), drv_alarm_isr, FALLING);
    Serial.println("[MOTOR] motor_ctrl inicijalizovan.");
    Serial.printf("[MOTOR] Pinovi: PUL=%d DIR=%d ENA=%d HOME=%d BEND=%d\n",
                  PIN_PUL, PIN_DIR, PIN_ENA, PIN_HOME, PIN_BEND);
}

// ── Pomak na apsolutnu poziciju ────────────────────────────────────────────
void motor_ctrl_move_abs(float pos_mm, float speed_mmps)
{
    if (s_state == MSTATE_ALARM)  return;
    if (s_state == MSTATE_HOMING) return;

    // Soft limits (samo kad smo homed)
    if (s_homed) {
        // Gornji limit: senzor strana — ne prelazi safe_max
        if (s_safe_max_valid) {
            if (pos_mm > s_safe_max_mm) pos_mm = s_safe_max_mm;
        } else {
            float fallback = g_motor_cfg.home_offset_mm - g_motor_cfg.home_clearance_mm;
            if (pos_mm > fallback) pos_mm = fallback;
        }
        // Donji limit: radna strana — ne ide ispod (home_offset - max_travel)
        float min_mm = g_motor_cfg.home_offset_mm - g_motor_cfg.max_travel_mm;
        if (pos_mm < min_mm) pos_mm = min_mm;
    }

    float spd          = (speed_mmps > 0) ? speed_mmps : g_motor_cfg.auto_speed_mmps;
    long  target_steps = mm_to_steps(pos_mm);

    // ── Jednosmerni dolazak (uvek iz smera ka komadu) ─────────────────────
    float current_mm = motor_get_position_mm();
    float dist_mm    = fabsf(pos_mm - current_mm);

    if (s_homed && g_motor_cfg.overshoot_mm > 0.001f && pos_mm < current_mm - 0.01f) {
        // Prekoracena pozicija: idi još bliže komadu (manji mm)
        float overshoot_pos = pos_mm - g_motor_cfg.overshoot_mm;
        float min_mm = g_motor_cfg.home_offset_mm - g_motor_cfg.max_travel_mm;
        if (overshoot_pos < min_mm) overshoot_pos = min_mm;

        long  ow_steps   = mm_to_steps(overshoot_pos);
        long  back_steps = target_steps - ow_steps;   // uvek pozitivno (ka motoru)
        float app_spd    = (g_motor_cfg.approach_speed_mmps > 0.5f)
                           ? g_motor_cfg.approach_speed_mmps
                           : g_motor_cfg.home_speed_mmps;

        // Paket 1: brzo do prekoracene tacke (bliže komadu)
        stepper.moveTo(ow_steps, speed_hz(spd), accel_hz());
        // Paket 2: sporo nazad ka motoru na tacnu poziciju
        stepper.moveRel(back_steps, speed_hz(app_spd), accel_hz());

        Serial.printf("[MOTOR] moveTo %.3fmm: prekoraci %.3fmm @ %.1fmm/s, vrati @ %.1fmm/s\n",
                      pos_mm, overshoot_pos, spd, app_spd);
    } else {
        // UDALJI ili mali pomak bez overshoot-a.
        // Ograniči brzinu tako da trapezoidal profil može da se razvije:
        //   v_max = sqrt(a × d)  →  za 1mm@300mm/s²: ~17mm/s, za 0.1mm: ~5.5mm/s
        if (dist_mm > 0.001f) {
            float v_cap = sqrtf(g_motor_cfg.accel_mmps2 * dist_mm);
            if (spd > v_cap) spd = v_cap;
        }
        stepper.moveTo(target_steps, speed_hz(spd), accel_hz());
        Serial.printf("[MOTOR] moveTo %.3f mm (%ld steps) @ %.1f mm/s\n",
                      pos_mm, target_steps, spd);
    }
}

// ── Relativni pomak (JOG) — ide kroz move_abs (sa overshoot-om) ──────────
void motor_ctrl_move_rel(float delta_mm, float speed_mmps)
{
    float cur = motor_get_position_mm();
    motor_ctrl_move_abs(cur + delta_mm, speed_mmps);
}

// ── Pokreni homing ────────────────────────────────────────────────────────
void motor_ctrl_home(void)
{
    if (s_state == MSTATE_HOMING) {
        Serial.println("[MOTOR] Homing vec u toku, ignorisano.");
        return;
    }
    stepper.stop();
    // Pokreni homing task na core 0 (stepper task je na core 1)
    BaseType_t rc = xTaskCreatePinnedToCore(
        homing_task, "homing",
        4096, nullptr,
        5,    nullptr,
        0     // core 0
    );
    if (rc != pdPASS) {
        Serial.println("[MOTOR] xTaskCreate homing GRESKA!");
    }
}

// ── Stop ─────────────────────────────────────────────────────────────────
void motor_ctrl_stop(void)
{
    stepper.softStop();   // Deceleriše po rampi — ne škodi motoru
    if (s_state == MSTATE_MOVING || s_state == MSTATE_IDLE) {
        s_state = MSTATE_IDLE;
    }
    Serial.println("[MOTOR] STOP.");
}

// ── Postavi nulu bez senzora (testiranje bez home switch-a) ─────────────
void motor_ctrl_set_zero(void)
{
    if (s_state == MSTATE_HOMING) {
        Serial.println("[MOTOR] SET_ZERO ignorisan - homing u toku.");
        return;
    }
    stepper.stop();
    long pos = mm_to_steps(g_motor_cfg.home_offset_mm);
    stepper.setPosition(pos);
    s_homed      = true;
    s_state      = MSTATE_IDLE;
    s_alarm_code = ALARM_NONE;
    Serial.printf("[MOTOR] SET_ZERO: pozicija = %.2f mm (simuliran homing)\n",
                  motor_get_position_mm());
}

// ── Home limit zaštita (pozivati ~50ms iz status taska) ─────────────────
// ISR postavlja s_home_irq odmah (~1–5ms), ovdje samo obrađujemo stanje
void motor_ctrl_limit_tick(void)
{
    if (!s_home_irq) return;
    if (s_state == MSTATE_HOMING) return;  // homing koristi vlastitu logiku
    // Debounce: potvrdi da je pin i dalje HIGH (senzor i dalje aktivan)
    if (digitalRead(PIN_HOME) != HIGH) { s_home_irq = false; return; }
    // Ako je motor parkiran pri nuli (odmah nakon hominga) — nije overtravel, ignoriši
    float cur_pos = motor_get_position_mm();
    if (cur_pos >= -1.0f && cur_pos <= 2.0f) { s_home_irq = false; return; }
    s_home_irq   = false;
    stepper.stop();
    s_state      = MSTATE_ALARM;
    s_alarm_code = ALARM_OVERTRAVEL;
    Serial.printf("[MOTOR] OVERTRAVEL @ %.2fmm! Alarm.\n", cur_pos);
}

// ── Driver ALM zaštita (pozivati ~50ms iz status taska) ─────────────────
void motor_ctrl_driver_alarm_tick(void)
{
    if (!s_drv_alarm_irq) return;
    if (s_state == MSTATE_HOMING) { s_drv_alarm_irq = false; return; }  // ignorisi tokom hominga
    // Debounce: potvrdi da je pin i dalje LOW (ALM aktivan)
    if (digitalRead(PIN_ALM) != LOW) { s_drv_alarm_irq = false; return; }
    s_drv_alarm_irq = false;
    stepper.stop();
    s_state      = MSTATE_ALARM;
    s_alarm_code = ALARM_DRIVER;
    Serial.println("[MOTOR] DRIVER ALM: drajver javio gresku! ALARM.");
}

// ── Retract auto-trigger (pozivati ~50ms iz status taska) ────────────────
void motor_ctrl_set_retract_enabled(bool en)
{
    s_retract_enabled = en;
    Serial.printf("[MOTOR] Retract %s\n", en ? "UKLJUCEN" : "ISKLJUCEN");
}

void motor_ctrl_bend_tick(void)
{
    bool bend_high = (digitalRead(PIN_BEND) == HIGH);
    // Rastući brid (LOW→HIGH) = kontakt prekinut = savijanje završeno → retract
    if (!s_last_bend_high && bend_high &&
        s_retract_enabled && s_homed &&
        motor_get_state() == MSTATE_IDLE &&
        g_motor_cfg.retract_mm > 0.1f)
    {
        float cur   = motor_get_position_mm();
        float spd   = (g_motor_cfg.retract_speed_mmps > 0.5f)
                      ? g_motor_cfg.retract_speed_mmps
                      : g_motor_cfg.auto_speed_mmps;
        // Retract u + smjeru (ka senzoru) ali ne prelazi safe_max
        float ret_target = cur + g_motor_cfg.retract_mm;
        if (s_safe_max_valid && ret_target > s_safe_max_mm) ret_target = s_safe_max_mm;
        float delta = ret_target - cur;
        if (delta > 0.001f) {
            long steps = mm_to_steps(delta);
            stepper.moveRel(steps, speed_hz(spd), accel_hz());
            Serial.printf("[MOTOR] RETRACT auto %.1fmm (capped %.1fmm) @ %.1fmm/s\n",
                          g_motor_cfg.retract_mm, delta, spd);
        }
    }
    s_last_bend_high = bend_high;
}

bool motor_get_bend(void)
{
    return (digitalRead(PIN_BEND) == HIGH);  // HIGH = kontakti otvoreni = savijanje
}

// ── Clear alarm ───────────────────────────────────────────────────────────
void motor_ctrl_clear_alarm(void)
{
    bool was_overtravel = (s_alarm_code == ALARM_OVERTRAVEL);
    s_home_irq      = false;
    s_drv_alarm_irq = false;
    s_alarm_code    = ALARM_NONE;
    s_state         = MSTATE_IDLE;
    if (was_overtravel) {
        // Motor je pri home senzoru = pozicija je 0mm, homing je i dalje validan
        long home_pos = mm_to_steps(g_motor_cfg.home_offset_mm);
        stepper.setPosition(home_pos);
        // s_homed ostaje TRUE — nema potrebe za novim homingom
        Serial.println("[MOTOR] Overtravel alarm obrisan. Poz=0mm. Homing validan.");
    } else {
        s_homed = false;   // drajver alarm i ostalo = nepoznata pozicija, treba re-home
        Serial.println("[MOTOR] Alarm obrisan. Treba homing.");
    }
    // Obnovi overtravel interrupt (RISING = NC switch otvoren = pin HIGH)
    s_home_irq = false;
    attachInterrupt(digitalPinToInterrupt(PIN_HOME), home_limit_isr, RISING);
}

// ── Primjena nove konfiguracije (CMD_SYNC) ────────────────────────────────
void motor_ctrl_apply_config(const MotorConfig &cfg)
{
    g_motor_cfg = cfg;
    Serial.printf("[MOTOR] Config: stp/mm=%.1f  max=%.1fmm  spd=%.1f  "
                  "acc=%.1f  home_spd=%.1f  offset=%.2f  dir=%d\n",
                  cfg.steps_per_mm, cfg.max_travel_mm,
                  cfg.auto_speed_mmps, cfg.accel_mmps2,
                  cfg.home_speed_mmps, cfg.home_offset_mm,
                  (int)cfg.direction);
}

// ── State čitanje ─────────────────────────────────────────────────────────
float motor_get_position_mm(void)
{
    // steps / (steps_per_mm * direction)
    return (float)stepper.getPosition() /
           (g_motor_cfg.steps_per_mm * (float)g_motor_cfg.direction);
}

uint8_t motor_get_state(void)
{
    // ALARM i HOMING su eksplicitni; ostalo = provjer stepper.isMoving()
    if (s_state == MSTATE_ALARM)  return MSTATE_ALARM;
    if (s_state == MSTATE_HOMING) return MSTATE_HOMING;
    return stepper.isMoving() ? MSTATE_MOVING : MSTATE_IDLE;
}

bool motor_is_homed(void)
{
    return s_homed;
}

uint8_t motor_get_alarm_code(void)
{
    return s_alarm_code;
}
