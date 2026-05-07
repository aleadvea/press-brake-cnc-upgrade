// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  StepperRMT.cpp — Implementacija (IDF 4.4 / Arduino ESP32 2.x)         ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#include "StepperRMT.h"
#include "esp_log.h"

static const char* TAG = "StepperRMT";

// ── Konstruktor ──────────────────────────────────────────────────────────────
StepperRMT::StepperRMT(uint8_t pinPUL, uint8_t pinDIR, uint8_t pinENA,
                       uint32_t stepsPerRev, rmt_channel_t rmtChannel)
    : _pinPUL(pinPUL), _pinDIR(pinDIR), _pinENA(pinENA),
      _stepsPerRev(stepsPerRev), _ch(rmtChannel)
{}

// ── begin() ──────────────────────────────────────────────────────────────────
void StepperRMT::begin() {
    // DIR i ENA kao digitalni izlazi; PUL kontroliše RMT hardver
    pinMode(_pinDIR, OUTPUT);
    // ENA: pocetno stanje = INPUT_PULLUP (high-Z) = optokapler OFF = motor prima korake
    // Ne koristimo OUTPUT HIGH jer 330Ω + 1.7V razlika daje 1.5mA → slabo upali optokapler
    pinMode(_pinENA, INPUT_PULLUP);
    digitalWrite(_pinDIR, LOW);

    // ── RMT TX kanal (IDF 4.4 legacy API) ───────────────────────────────
    // APB clock = 80 MHz; clk_div=80 → 1 MHz rezolucija → 1 µs/tick
    rmt_config_t cfg = {};
    cfg.rmt_mode                  = RMT_MODE_TX;
    cfg.channel                   = _ch;
    cfg.gpio_num                  = (gpio_num_t)_pinPUL;
    cfg.clk_div                   = RMT_CLK_DIV;      // 80 → 1 µs/tick
    cfg.mem_block_num             = 1;                 // 64 item hardware RAM
    cfg.tx_config.loop_en         = false;
    cfg.tx_config.carrier_en      = false;
    cfg.tx_config.idle_output_en  = true;
    cfg.tx_config.idle_level      = RMT_IDLE_LEVEL_HIGH; // PUL HIGH u mirovanju = optocoupler OFF = nema impulsa
    ESP_ERROR_CHECK(rmt_config(&cfg));
    ESP_ERROR_CHECK(rmt_driver_install(_ch, 0, 0));

    // ── FreeRTOS ─────────────────────────────────────────────────────────
    _queue = xQueueCreate(16, sizeof(MotorMove));
    configASSERT(_queue != nullptr);

    // Prikačimo task na core 1; core 0 = Arduino loop() + WiFi
    // Prioritet 10: viši od Arduino loop (1) i većine sistemskih taskova (do 7)
    // Stack 8192: potrebno za float S-kriva računice + FreeRTOS overhead
    BaseType_t rc = xTaskCreatePinnedToCore(
        _taskFn, "stepper_rmt",
        8192,      // stack (float operacije + sigurnosna margina)
        this,
        10,        // prioritet (viši = manje jittera između chunkova)
        &_taskHnd,
        1          // core 1
    );
    configASSERT(rc == pdPASS);

    ESP_LOGI(TAG, "StepperRMT spreman. PUL=%u DIR=%u ENA=%u ch=%d spr=%u",
             _pinPUL, _pinDIR, _pinENA, (int)_ch, _stepsPerRev);
}

// ── enable / disable ─────────────────────────────────────────────────────────
void StepperRMT::enable() {
    // ENA nije vezan na ESP32 (P108=0001 — drajver ignoriše ENA pin)
    // Motor je uvijek aktivan dok god postoji napajanje drajvera
    _enabled.store(true);
    ESP_LOGI(TAG, "Motor ENABLED (P108=0001, ENA hardverski neaktivan)");
}

void StepperRMT::disable() {
    // Bez fizičke ENA kontrole — jedini način zaustavljanja je stop() ili E-STOP na PUL+
    _enabled.store(false);
    ESP_LOGW(TAG, "Motor DISABLED (samo softverski — ENA nije vezan)");
}

// ── Javne naredbe kretanja ────────────────────────────────────────────────────
void StepperRMT::moveRel(long steps, float vMax, float accel) {
    MotorMove m = { steps, vMax, accel };
    xQueueSend(_queue, &m, portMAX_DELAY);
}

void StepperRMT::moveTo(long absPos, float vMax, float accel) {
    moveRel(absPos - _pos.load(), vMax, accel);
}

void StepperRMT::stop() {
    _stop.store(true);
    xQueueReset(_queue);
    ESP_LOGW(TAG, "STOP zahtjev");
}

void StepperRMT::softStop() {
    xQueueReset(_queue);          // Ukloni buduće pokrete
    _soft_stop.store(true);       // Signal _execMove da deceleriše
    ESP_LOGW(TAG, "SOFT STOP zahtjev");
}

void IRAM_ATTR StepperRMT::stopFromISR() {
    // Samo atomski flag — xQueueReset nije ISR-safe, poziva se iz task konteksta
    _stop.store(true, std::memory_order_seq_cst);
}

void StepperRMT::waitDone() {
    // Kratka pauza da task stigne dequeue-ovati i postaviti _moving=true
    vTaskDelay(pdMS_TO_TICKS(25));
    while (_moving.load() || uxQueueMessagesWaiting(_queue) > 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ── _sendChunk ────────────────────────────────────────────────────────────────
// Generiše n koračnih impulsa hardware-ski putem RMT.
// Blokira motor-task (ne main loop) dok tranzicija nije gotova.
void StepperRMT::_sendChunk(uint32_t n, uint32_t periodTicks, int8_t dir) {
    // DIR je vec postavljen u _execMove prije prvog chunk-a — ne postavljamo ovdje
    // (izbjegavamo 10us delayMicroseconds pauzu unutar hot-path petlje)

    // Izračunaj HIGH/LOW trajanje impulsa
    // Pr. pri 3000 Hz: period=333µs → hi=5µs, lo=328µs
    const uint32_t hi = PULSE_TICKS;
    const uint32_t lo = (periodTicks > hi + 1u) ? (periodTicks - hi) : 1u;

    // Popuni buffer (rmt_item32_t: 2×16-bit u jednom 32-bit word-u)
    for (uint32_t i = 0; i < n; i++) {
        _buf[i].duration0 = hi;   // PUL LOW  = optocoupler ON = aktivni puls (5 µs)
        _buf[i].level0    = 0;    // LOW
        _buf[i].duration1 = lo;   // PUL HIGH = optocoupler OFF = idle
        _buf[i].level1    = 1;    // HIGH
    }

    // Transmitiraj; wait_tx_done=true → blokira dok svi pulzevi nisu poslati
    rmt_write_items(_ch, _buf, (int)n, true);

    // Ažuriraj poziciju (atomski read-modify-write)
    _pos.fetch_add((long)dir * (long)n);
}

// ── _execMove ────────────────────────────────────────────────────────────────
// Trapezoidalni profil brzine (linearno ubrzanje/usporenje):
//
//  Hz
//  vMax  ──────────────────────────────────
//         /                                \
//        /  ubrzanje         usporenje      \
//  vMin ─/                                   \─
//        |← nRamp →|← const. koraci →|← nRamp →|
//
// nRamp ≈ (vMax² - vMin²) / (2 × accel)
// Izvod: v² = v₀² + 2·a·n (u Hz/korak domenu)
// accel [Hz/s] = fizička mm/s² × STEPS_PER_MM
// Pr.: 300 mm/s² × 640 = 192000 Hz/s → rampa 50mm/s za ~0.16s
//
void StepperRMT::_execMove(const MotorMove& m) {
    if (m.steps == 0) return;

    _moving.store(true);
    _stop.store(false);
    _soft_stop.store(false);

    const int8_t dir  = (m.steps > 0) ? 1 : -1;
    long   absTotal   = (m.steps < 0) ? -m.steps : m.steps;
    bool   softStopping = false;

    // Ograničimo parametre na razumne vrijednosti
    // vMin=200Hz: ispod 200Hz (0.31mm/s) motor ulazi u mehaničku rezonantnu zonu → vibracije
    const float vMin  = 200.0f;
    const float vMax  = (m.vMax  > vMin)  ? m.vMax  : vMin;
    const float acc   = (m.accel > 1.0f)  ? m.accel : 1.0f;

    // Postavi DIR jednom ovdje — ZDM trazi DIR stabilan >=5us prije prvog PUL
    // Ne postavljamo unutar _sendChunk jer to uzrokuje 10us pauzu na svakom chunk-u
    digitalWrite(_pinDIR, dir > 0 ? LOW : HIGH);
    delayMicroseconds(10);

    // Broj koraka za rampiranje od vMin do vMax
    long nRamp = (long)((vMax * vMax - vMin * vMin) / (2.0f * acc));
    if (nRamp < 1L)           nRamp = 1L;
    if (nRamp > absTotal / 2) nRamp = absTotal / 2;

    for (long done = 0; done < absTotal && !_stop.load(); ) {
        const long remaining = absTotal - done;

        float speed;
        bool  inRamp;

        if (done < nRamp) {
            // Faza ubrzanja: linearno 0→1 (trapezoidalni profil)
            float t  = (float)done / (float)nRamp;
            speed    = vMin + t * (vMax - vMin);
            inRamp   = true;
        } else if (remaining <= nRamp) {
            // Faza usporenja: linearno 1→0 (trapezoidalni profil)
            float t  = (float)remaining / (float)nRamp;
            speed    = vMin + t * (vMax - vMin);
            inRamp   = true;
        } else {
            // Konstantna brzina
            speed  = vMax;
            inRamp = false;
        }

        if (speed < vMin) speed = vMin;

        // ── Soft stop: kreni s deceleracijom iz trenutne brzine ───────
        if (!softStopping && _soft_stop.load()) {
            _soft_stop.store(false);
            // Izračunaj broj koraka potrebnih za usporenje od speed → vMin
            long nDecel = (long)((speed * speed - vMin * vMin) / (2.0f * acc));
            if (nDecel < 1L) nDecel = 1L;
            absTotal = done + nDecel;   // novi kraj = upravo toliko koraka decela
            nRamp    = nDecel;          // cijela preostala dionica je decel rampa
            softStopping = true;
        }

        // ── Veličina chunk-a ──────────────────────────────────────────
        // Fine granulacija tokom rampe (glatka kriva), gruba pri konst. brzini
        uint32_t chunk = inRamp ? CHUNK_RAMP : CHUNK_CONST;
        if ((long)chunk > remaining) chunk = (uint32_t)remaining;

        // ── Perioda impulsa ───────────────────────────────────────────
        // periodTicks = 1_000_000 / speed [µs]
        // Pr.: 3000 Hz → 333 µs period → hi=5µs, lo=328µs
        uint32_t periodTicks = (uint32_t)(RMT_RES_HZ / speed);

        _sendChunk(chunk, periodTicks, dir);
        done += chunk;
    }

    _moving.store(false);
    _stop.store(false);
}

// ── FreeRTOS task ─────────────────────────────────────────────────────────────
void StepperRMT::_taskFn(void* arg) {
    auto* self = static_cast<StepperRMT*>(arg);
    MotorMove m;
    for (;;) {
        // Čekaj naredbu neograničeno dugo (ne vrte CPU)
        if (xQueueReceive(self->_queue, &m, portMAX_DELAY) == pdTRUE) {
            self->_execMove(m);
        }
    }
}
