#pragma once
#include <stdint.h>
#include <stdbool.h>

// ====================================================================
//  espnow_motor.h — ESP-NOW komunikacija, motor strana
//
//  Isti protokol kao espnow_hmi.h (HMI strana):
//   Primanje: CmdPacket / SyncPacket od HMI → izvršava na motoru
//   Slanje:   StatusPacket → HMI svakih ~50ms (iz status_task u main)
// ====================================================================

// ── Komande ───────────────────────────────────────────────────────────────
#define CMD_MOVE_ABS  ((uint8_t)1)
#define CMD_MOVE_REL  ((uint8_t)2)
#define CMD_HOME      ((uint8_t)3)
#define CMD_STOP      ((uint8_t)4)
#define CMD_SYNC      ((uint8_t)5)
#define CMD_SET_ZERO    ((uint8_t)6)   // postavi nulu bez senzora
#define CMD_CLEAR_ALARM ((uint8_t)7)   // reset alarm stanja
#define CMD_RESTART     ((uint8_t)8)   // restart uredjaja

#define ALARM_NONE       0
#define ALARM_HOME_FAIL  1
#define ALARM_SOFT_LIMIT 2
#define ALARM_DRIVER     3
#define ALARM_OVERTRAVEL 4

// ── Paketi ────────────────────────────────────────────────────────────────
typedef struct __attribute__((packed)) {
    uint8_t cmd;
    float   value_mm;
    float   speed_mmps;
} CmdPacket;

typedef struct __attribute__((packed)) {
    uint8_t cmd;
    float   steps_per_mm;
    float   max_travel_mm;
    float   home_speed_mmps;
    float   home_fast_speed_mmps;
    float   home_odmak_mm;
    float   home_clearance_mm;   // odmak od senzora po zavrsetku hominga
    float   auto_speed_mmps;
    float   accel_mmps2;
    float   home_offset_mm;
    float   overshoot_mm;
    float   approach_speed_mmps;
    float   retract_mm;
    float   retract_speed_mmps;
    uint8_t retract_enabled;
    int8_t  direction;
} SyncPacket;

typedef struct __attribute__((packed)) {
    float   position_mm;
    uint8_t motor_state;
    uint8_t is_homed;
    uint8_t alarm_code;
    uint8_t bend_sensor;   // 1 = kontakti otvoreni = savijanje aktivno (NC prekidac)
} StatusPacket;

// ── API ───────────────────────────────────────────────────────────────────
void espnow_motor_init(void);
void espnow_motor_send_status(void);   // poziva status_task svake 50ms
