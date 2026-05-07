#pragma once
#include <stdint.h>
#include <stdbool.h>

// ====================================================================
//  motor_ctrl — Upravljanje koračnim motorom
//
//  Koristi StepperRMT lib (RMT hardverski pulzevi, FreeRTOS task core 1)
//  Homing: dvofazna sekvenca (brzo + sporo traženje senzora)
//  Soft limits: odbija pokrete van [0, max_travel_mm] (samo kad je homed)
// ====================================================================

// ── Pinout GPIO ───────────────────────────────────────────────────────────
#define PIN_PUL   18
#define PIN_DIR   19
#define PIN_ENA   21
#define PIN_HOME  22
#define PIN_BEND  23
#define PIN_ALM   34   // Drajver ALM+ (HIGH=ok, LOW=alarm)

// ── Stanja motora ─────────────────────────────────────────────────────────
#define MSTATE_IDLE    0
#define MSTATE_MOVING  1
#define MSTATE_HOMING  2
#define MSTATE_ALARM   3

// ── Alarm kodovi ──────────────────────────────────────────────────────────
#define ALARM_NONE       0
#define ALARM_HOME_FAIL  1   // homing greška (senzor nije pronađen)
#define ALARM_SOFT_LIMIT 2
#define ALARM_DRIVER     3   // Drajver ALM signal (overcurrent/overtemp)
#define ALARM_OVERTRAVEL 4   // home senzor pogođen za vrijeme normalnog rada

// ── Konfiguracija (prima se od HMI putem CMD_SYNC) ────────────────────────
struct MotorConfig {
    float  steps_per_mm;     // 640.0  (3200 spr / 5mm navoj)
    float  max_travel_mm;    // 500.0
    float  home_speed_mmps;  // 20.0   (spora faza homing-a)
    float  auto_speed_mmps;  // 80.0
    float  accel_mmps2;      // 300.0
    float  home_offset_mm;    // 0.0    (pozicija senzora → logicka 0)
    float  home_fast_speed_mmps; // 100.0  brza faza homing-a
    float  home_odmak_mm;        // 5.0    odmak izmedju brze i spore faze
    float  home_clearance_mm;    // 5.0    sigurnosni odmak od senzora po homingu
    float  overshoot_mm;         // 2.0    prekoracenje za jednosmerni dolazak
    float  approach_speed_mmps;  // 10.0   brzina finalnog prilaza
    float  retract_mm;           // 2.0    distanca auto-retrakta
    float  retract_speed_mmps;   // 50.0   brzina retrakta
    int8_t direction;            // 1 = normalno, -1 = invertirano
};

// Globalna konfiguracija (defaults u motor_ctrl.cpp; ažurira se CMD_SYNC)
extern MotorConfig g_motor_cfg;

// ── API ───────────────────────────────────────────────────────────────────
void    motor_ctrl_init(void);

// Pomaci (speed_mmps = 0 → koristi g_motor_cfg.auto_speed_mmps)
void    motor_ctrl_move_abs(float pos_mm,   float speed_mmps);
void    motor_ctrl_move_rel(float delta_mm, float speed_mmps);
void    motor_ctrl_home(void);
void    motor_ctrl_stop(void);
void    motor_ctrl_set_zero(void);    // postavi nulu bez senzora (testiranje)
void    motor_ctrl_clear_alarm(void);
void    motor_ctrl_apply_config(const MotorConfig &cfg);
void    motor_ctrl_set_retract_enabled(bool en);
void    motor_ctrl_limit_tick(void);
void    motor_ctrl_driver_alarm_tick(void);
void    motor_ctrl_bend_tick(void);

// State čitanje (poziva se iz espnow_motor.cpp za StatusPacket)
float   motor_get_position_mm(void);
uint8_t motor_get_state(void);
bool    motor_is_homed(void);
bool    motor_get_bend(void);        // true = kontakti otvoreni = savijanje aktivno
uint8_t motor_get_alarm_code(void);
