#pragma once
#include <stdint.h>
#include <string.h>

// ------------------------------------------------------------------ //
//  Konstante                                                          //
// ------------------------------------------------------------------ //
#define MAX_MATERIALS   50
#define MAX_PROGRAMS    50
#define MAX_STEPS       20
#define NAME_LEN        32

// ------------------------------------------------------------------ //
//  Material                                                           //
// ------------------------------------------------------------------ //
struct Material {
    char    name[NAME_LEN];
    float   thickness_mm;
    float   offset_mm;
};

// ------------------------------------------------------------------ //
//  Program step - jedna pozicija savijanja                            //
// ------------------------------------------------------------------ //
struct ProgramStep {
    float   position_mm;
};

// ------------------------------------------------------------------ //
//  Program                                                            //
// ------------------------------------------------------------------ //
struct Program {
    char        name[NAME_LEN];
    int         material_idx;       // indeks u g_materials, -1 = nema
    int         step_count;
    ProgramStep steps[MAX_STEPS];
};

// ------------------------------------------------------------------ //
//  Settings                                                           //
// ------------------------------------------------------------------ //
struct Settings {
    float   steps_per_mm;         // koraci motora po mm (640 = 3200spr/5mm)
    float   max_travel_mm;        // maksimalni hod (mm)
    float   retract_mm;           // retract razmak (mm)
    float   retract_speed_mmps;   // brzina retrakta (mm/s)
    float   jog_speed_mmps;       // brzina jog-a (mm/s)
    float   auto_speed_mmps;      // brzina auto moda (mm/s)
    float   home_speed_mmps;      // homing spora faza (mm/s)
    float   home_fast_speed_mmps; // homing brza faza (mm/s)
    float   home_odmak_mm;        // odmak izmedju brze i spore faze (mm)
    float   home_clearance_mm;    // sigurnosni odmak od senzora nakon hominga (mm)
    float   accel_mmps2;          // akceleracija (mm/s²)
    float   home_offset_mm;       // offset senzor→centar savijanja (mm)
    float   overshoot_mm;         // prekoracenje za jednosmerni dolazak (mm)
    float   approach_speed_mmps;  // brzina finalnog prilaza (mm/s)
    float   auto_retract_pause_s; // pauza na retraktu prije sljedeceg koraka (s)
    int     direction;            // 1 = normalno, -1 = invertirano
};

// ------------------------------------------------------------------ //
//  Runtime masinska pozicija (nije persistentna)                     //
// ------------------------------------------------------------------ //
struct MachineState {
    float   position_mm;
    uint8_t motor_state;   // MSTATE_* (0=IDLE,1=MOVING,2=HOMING,3=ALARM)
    bool    is_homed;
    bool    alarm_active;
    bool    home_sensor;
    bool    bend_sensor;
    bool    motor_ok;
    char    alarm_msg[64];
};

// ------------------------------------------------------------------ //
//  Globalni podaci (definisani u storage.cpp)                         //
// ------------------------------------------------------------------ //
extern Material     g_materials[MAX_MATERIALS];
extern int          g_material_count;

extern Program      g_programs[MAX_PROGRAMS];
extern int          g_program_count;

extern Settings     g_settings;
extern MachineState g_machine;
extern bool         g_retract_enabled;  // runtime flag (nije snimljeno)

// Pomocne funkcije
inline void machine_state_init(MachineState &s) {
    s.position_mm  = 0.0f;
    s.motor_state  = 0;
    s.is_homed     = false;
    s.alarm_active = false;
    s.home_sensor  = false;
    s.bend_sensor  = false;
    s.motor_ok     = true;
    s.alarm_msg[0] = '\0';
}

inline void settings_defaults(Settings &s) {
    s.steps_per_mm    = 640.0f;   // 3200 spr / 5mm navoj
    s.max_travel_mm   = 500.0f;
    s.retract_mm           = 2.0f;
    s.retract_speed_mmps   = 20.0f;
    s.jog_speed_mmps       = 30.0f;
    s.auto_speed_mmps = 30.0f;
    s.home_speed_mmps      = 5.0f;
    s.home_fast_speed_mmps = 20.0f;
    s.home_odmak_mm        = 5.0f;
    s.home_clearance_mm    = 5.0f;
    s.accel_mmps2          = 150.0f;
    s.home_offset_mm     = 0.0f;
    s.overshoot_mm       = 1.0f;
    s.approach_speed_mmps= 5.0f;
    s.auto_retract_pause_s = 5.0f;
    s.direction          = 1;
}
