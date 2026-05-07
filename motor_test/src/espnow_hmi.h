#pragma once
#include <stdint.h>
#include <stdbool.h>

// ====================================================================
//  ESP-NOW protokol — HMI strana
//
//  HMI (ESP32-S3)  ──────────────►  Motor (ESP32 WROOM-32)
//                    CmdPacket / SyncPacket  (broadcast ili unicast)
//
//  Motor (ESP32 WROOM-32)  ──────►  HMI (ESP32-S3)
//                    StatusPacket  svakih ~50 ms
// ====================================================================

// ── Komande (HMI → Motor) ─────────────────────────────────────────────────
#define CMD_MOVE_ABS  ((uint8_t)1)   // apsolutni pomak na poziciju
#define CMD_MOVE_REL  ((uint8_t)2)   // relativni jog pomak
#define CMD_HOME      ((uint8_t)3)   // pokreni homing sekvencu
#define CMD_STOP      ((uint8_t)4)   // hitni stop
#define CMD_SYNC      ((uint8_t)5)   // sinhroniziraj podesavanja
#define CMD_SET_ZERO    ((uint8_t)6)   // postavi nulu bez senzora (testiranje)
#define CMD_CLEAR_ALARM ((uint8_t)7)   // reset alarm stanja na motoru
#define CMD_RESTART     ((uint8_t)8)   // restart oba ESP32 uredjaja

// ── Alarm kodovi (dolaze u StatusPacket.alarm_code) ───────────────────────
#define ALARM_NONE       0
#define ALARM_HOME_FAIL  1   // homing greška
#define ALARM_SOFT_LIMIT 2
#define ALARM_DRIVER     3   // Drajver ALM: overcurrent/overtemp
#define ALARM_OVERTRAVEL 4   // home senzor pogođen za vrijeme rada

// ── Stanja motora (dolaze u StatusPacket.motor_state) ─────────────────────
#define MSTATE_IDLE    0
#define MSTATE_MOVING  1
#define MSTATE_HOMING  2
#define MSTATE_ALARM   3

// ── Paket: pokret (9 bajtova) ─────────────────────────────────────────────
typedef struct __attribute__((packed)) {
    uint8_t cmd;          // CMD_MOVE_ABS ili CMD_MOVE_REL
    float   value_mm;     // pozicija (ABS) ili delta (REL)
    float   speed_mmps;   // 0 = koristiti default brzinu iz podesavanja
} CmdPacket;

// ── Paket: sinhronizacija podesavanja (30 bajtova) ────────────────────────
typedef struct __attribute__((packed)) {
    uint8_t cmd;              // = CMD_SYNC
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

// ── Paket: status motora (8 bajtova) ─────────────────────────────────────
typedef struct __attribute__((packed)) {
    float   position_mm;
    uint8_t motor_state;   // MSTATE_*
    uint8_t is_homed;      // bool kao uint8
    uint8_t alarm_code;    // 0 = nema alarma
    uint8_t bend_sensor;   // 1 = kontakti otvoreni = savijanje aktivno (NC)
} StatusPacket;

// ── API ───────────────────────────────────────────────────────────────────
void espnow_hmi_init(void);
void espnow_hmi_set_motor_mac(const uint8_t mac[6]);

bool espnow_hmi_send_move_abs(float pos_mm, float speed_mmps);
bool espnow_hmi_send_move_rel(float delta_mm, float speed_mmps);
bool espnow_hmi_send_home(void);
bool espnow_hmi_send_stop(void);
bool espnow_hmi_send_sync(void);      // šalje g_settings → motor
bool espnow_hmi_send_set_zero(void);  // postavi nulu bez senzora (testiranje)
bool espnow_hmi_send_clear_alarm(void); // reset alarm na motoru + ponovo homing
bool espnow_hmi_send_restart(void);     // restart motor strane (i potom HMI)
