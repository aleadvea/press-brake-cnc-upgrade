#include "storage.h"
#include "data_model.h"
#include "ui_strings.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// ------------------------------------------------------------------ //
//  Globalni podaci                                                    //
// ------------------------------------------------------------------ //
Material     g_materials[MAX_MATERIALS];
int          g_material_count = 0;

Program      g_programs[MAX_PROGRAMS];
int          g_program_count = 0;

Settings     g_settings;
MachineState g_machine;
bool         g_retract_enabled = true;

// ------------------------------------------------------------------ //
//  Init                                                               //
// ------------------------------------------------------------------ //
bool storage_init()
{
    machine_state_init(g_machine);
    settings_defaults(g_settings);

    if (!LittleFS.begin(true)) {   // true = formatira ako je prazan
        Serial.println("[STORAGE] LittleFS mount failed");
        return false;
    }
    Serial.println("[STORAGE] LittleFS OK");
    return true;
}

// ------------------------------------------------------------------ //
//  Load sve                                                           //
// ------------------------------------------------------------------ //
void storage_load_all()
{
    // -- Language --
    storage_load_language();
    
    // -- Settings --
    if (LittleFS.exists("/settings.json")) {
        File f = LittleFS.open("/settings.json", "r");
        JsonDocument doc;
        if (deserializeJson(doc, f) == DeserializationError::Ok) {
            g_settings.steps_per_mm    = doc["steps_per_mm"]    | 640.0f;
            g_settings.max_travel_mm   = doc["max_travel_mm"]   | 500.0f;
            g_settings.retract_mm         = doc["retract_mm"]         | 2.0f;
            g_settings.retract_speed_mmps   = doc["retract_speed_mmps"]  | 50.0f;
            g_settings.jog_speed_mmps       = doc["jog_speed_mmps"]      | 30.0f;
            g_settings.auto_speed_mmps = doc["auto_speed_mmps"] | 30.0f;
            g_settings.home_speed_mmps      = doc["home_speed_mmps"]      | 5.0f;
            g_settings.home_fast_speed_mmps  = doc["home_fast_speed_mmps"] | 20.0f;
            g_settings.home_odmak_mm         = doc["home_odmak_mm"]        | 5.0f;
            g_settings.home_clearance_mm     = doc["home_clearance_mm"]    | 5.0f;
            g_settings.accel_mmps2           = doc["accel_mmps2"]          | 150.0f;
            g_settings.home_offset_mm     = doc["home_offset_mm"]     | 0.0f;
            g_settings.overshoot_mm       = doc["overshoot_mm"]       | 2.0f;
            g_settings.approach_speed_mmps= doc["approach_speed_mmps"]| 10.0f;
            g_settings.auto_retract_pause_s = doc["auto_retract_pause_s"] | 5.0f;
            g_settings.direction          = doc["direction"]          | 1;
        }
        f.close();
    }

    // -- Materials --
    g_material_count = 0;
    if (LittleFS.exists("/materials.json")) {
        File f = LittleFS.open("/materials.json", "r");
        JsonDocument doc;
        if (deserializeJson(doc, f) == DeserializationError::Ok) {
            JsonArray arr = doc["materials"].as<JsonArray>();
            for (JsonObject obj : arr) {
                if (g_material_count >= MAX_MATERIALS) break;
                Material &m = g_materials[g_material_count++];
                strlcpy(m.name, obj["name"] | "Nepoznat", NAME_LEN);
                m.thickness_mm = obj["thickness_mm"] | 1.0f;
                m.offset_mm    = obj["offset_mm"]    | 0.0f;
            }
        }
        f.close();
    }

    // -- Programs --
    g_program_count = 0;
    if (LittleFS.exists("/programs.json")) {
        File f = LittleFS.open("/programs.json", "r");
        JsonDocument doc;
        if (deserializeJson(doc, f) == DeserializationError::Ok) {
            JsonArray arr = doc["programs"].as<JsonArray>();
            for (JsonObject obj : arr) {
                if (g_program_count >= MAX_PROGRAMS) break;
                Program &p = g_programs[g_program_count++];
                strlcpy(p.name, obj["name"] | "Program", NAME_LEN);
                p.material_idx = obj["material_idx"] | -1;
                p.step_count   = 0;
                JsonArray steps = obj["steps"].as<JsonArray>();
                for (JsonObject st : steps) {
                    if (p.step_count >= MAX_STEPS) break;
                    p.steps[p.step_count++].position_mm = st["position_mm"] | 0.0f;
                }
            }
        }
        f.close();
    }

    Serial.printf("[STORAGE] Ucitano: %d materijala, %d programa\n",
                  g_material_count, g_program_count);
}

// ------------------------------------------------------------------ //
//  Save settings                                                      //
// ------------------------------------------------------------------ //
void storage_save_settings()
{
    JsonDocument doc;
    doc["steps_per_mm"]    = g_settings.steps_per_mm;
    doc["max_travel_mm"]   = g_settings.max_travel_mm;
    doc["retract_mm"]         = g_settings.retract_mm;
    doc["retract_speed_mmps"]  = g_settings.retract_speed_mmps;
    doc["jog_speed_mmps"]      = g_settings.jog_speed_mmps;
    doc["auto_speed_mmps"] = g_settings.auto_speed_mmps;
    doc["home_speed_mmps"]      = g_settings.home_speed_mmps;
    doc["home_fast_speed_mmps"] = g_settings.home_fast_speed_mmps;
    doc["home_odmak_mm"]        = g_settings.home_odmak_mm;
    doc["home_clearance_mm"]    = g_settings.home_clearance_mm;
    doc["accel_mmps2"]          = g_settings.accel_mmps2;
    doc["home_offset_mm"]     = g_settings.home_offset_mm;
    doc["overshoot_mm"]       = g_settings.overshoot_mm;
    doc["approach_speed_mmps"]= g_settings.approach_speed_mmps;
    doc["auto_retract_pause_s"] = g_settings.auto_retract_pause_s;
    doc["direction"]          = g_settings.direction;

    File f = LittleFS.open("/settings.json", "w");
    serializeJson(doc, f);
    f.close();
}

// ------------------------------------------------------------------ //
//  Save materials                                                     //
// ------------------------------------------------------------------ //
void storage_save_materials()
{
    JsonDocument doc;
    JsonArray arr = doc["materials"].to<JsonArray>();
    for (int i = 0; i < g_material_count; i++) {
        JsonObject obj      = arr.add<JsonObject>();
        obj["name"]         = g_materials[i].name;
        obj["thickness_mm"] = g_materials[i].thickness_mm;
        obj["offset_mm"]    = g_materials[i].offset_mm;
    }
    File f = LittleFS.open("/materials.json", "w");
    serializeJson(doc, f);
    f.close();
}

// ------------------------------------------------------------------ //
//  Save programs                                                      //
// ------------------------------------------------------------------ //
void storage_save_programs()
{
    JsonDocument doc;
    JsonArray arr = doc["programs"].to<JsonArray>();
    for (int i = 0; i < g_program_count; i++) {
        Program &p      = g_programs[i];
        JsonObject obj  = arr.add<JsonObject>();
        obj["name"]          = p.name;
        obj["material_idx"]  = p.material_idx;
        JsonArray steps = obj["steps"].to<JsonArray>();
        for (int s = 0; s < p.step_count; s++) {
            JsonObject st     = steps.add<JsonObject>();
            st["position_mm"] = p.steps[s].position_mm;
        }
    }
    File f = LittleFS.open("/programs.json", "w");
    serializeJson(doc, f);
    f.close();
}

// ------------------------------------------------------------------ //
//  Load language                                                      //
// ------------------------------------------------------------------ //
void storage_load_language()
{
    if (LittleFS.exists("/language.json")) {
        File f = LittleFS.open("/language.json", "r");
        JsonDocument doc;
        if (deserializeJson(doc, f) == DeserializationError::Ok) {
            int lang = doc["language"] | 0;  // Default LANG_SERBIAN
            g_language = (Language)lang;
        }
        f.close();
    }
}

// ------------------------------------------------------------------ //
//  Save language                                                      //
// ------------------------------------------------------------------ //
void storage_save_language()
{
    JsonDocument doc;
    doc["language"] = (int)g_language;
    File f = LittleFS.open("/language.json", "w");
    serializeJson(doc, f);
    f.close();
}
