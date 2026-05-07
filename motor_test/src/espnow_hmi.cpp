// ====================================================================
//  espnow_hmi.cpp — ESP-NOW komunikacija, HMI strana
//
//  Primanje:  StatusPacket od motora → ažurira g_machine
//  Slanje:    CmdPacket / SyncPacket na motor MAC (broadcast default)
//
//  NAPOMENA: WiFi+ESP-NOW recv callback se izvršava u WiFi tasku.
//  Pisanje u g_machine (32-bit aligned float/uint8) je atomičan na
//  Xtensa LX7 (ESP32-S3) pa nije potreban mutex za ove podatke.
// ====================================================================
#include "espnow_hmi.h"
#include "data_model.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <string.h>

// Motor Brain MAC: B0:CB:D8:CF:57:60
static uint8_t s_motor_mac[6] = {0xB0, 0xCB, 0xD8, 0xCF, 0x57, 0x60};
static bool    s_initialized  = false;

// ── Primanje statusa od motora ─────────────────────────────────────────────
static void on_recv(const esp_now_recv_info_t *info,
                    const uint8_t             *data,
                    int                        len)
{
    (void)info;
    if (len < (int)sizeof(StatusPacket)) return;
    const StatusPacket *pkt = (const StatusPacket *)data;

    g_machine.position_mm  = pkt->position_mm;
    g_machine.motor_state  = pkt->motor_state;
    g_machine.is_homed     = (bool)pkt->is_homed;
    g_machine.motor_ok     = (pkt->motor_state != MSTATE_ALARM);
    g_machine.bend_sensor  = (bool)pkt->bend_sensor;

    if (pkt->alarm_code != ALARM_NONE) {
        g_machine.alarm_active = true;
        switch (pkt->alarm_code) {
            case ALARM_HOME_FAIL:  snprintf(g_machine.alarm_msg, sizeof(g_machine.alarm_msg), "HOMING GRESKA - senzor nije detektovan");          break;
            case ALARM_SOFT_LIMIT: snprintf(g_machine.alarm_msg, sizeof(g_machine.alarm_msg), "SOFT LIMIT - van opsega kretanja");                break;
            case ALARM_DRIVER:     snprintf(g_machine.alarm_msg, sizeof(g_machine.alarm_msg), "DRAJVER GRESKA - provjeri motor i kabl!");         break;
            case ALARM_OVERTRAVEL: snprintf(g_machine.alarm_msg, sizeof(g_machine.alarm_msg), "LIMIT! Pomjerite motor prema komadu pritiskom PRIBLIZI"); break;
            default:               snprintf(g_machine.alarm_msg, sizeof(g_machine.alarm_msg), "NEPOZNAT ALARM (kod %d)", pkt->alarm_code);        break;
        }
    } else {
        // Motor ocistio alarm
        g_machine.alarm_active = false;
    }
}

// ── Inicijalizacija ───────────────────────────────────────────────────────
void espnow_hmi_init(void)
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    esp_err_t err = esp_now_init();
    if (err != ESP_OK) {
        Serial.printf("[ESP-NOW] Init greška: 0x%X\n", (unsigned)err);
        return;
    }

    esp_now_register_recv_cb(on_recv);

    // Broadcast peer (koristimo dok ne naučimo pravi MAC od motora)
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, s_motor_mac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    s_initialized = true;
    Serial.println("[ESP-NOW] HMI spreman.");
    Serial.printf("[ESP-NOW] HMI MAC: %s\n", WiFi.macAddress().c_str());
    Serial.println("[ESP-NOW] Sada pokreni motor_brain i procitaj njegov MAC.");
}

// ── Postavi MAC motora (korisno kad znamo tačan MAC) ─────────────────────
void espnow_hmi_set_motor_mac(const uint8_t mac[6])
{
    if (s_initialized) {
        esp_now_del_peer(s_motor_mac);
    }
    memcpy(s_motor_mac, mac, 6);
    if (s_initialized) {
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, s_motor_mac, 6);
        peer.channel = 0;
        peer.encrypt = false;
        esp_now_add_peer(&peer);
    }
}

// ── Komande ───────────────────────────────────────────────────────────────
bool espnow_hmi_send_move_abs(float pos_mm, float speed_mmps)
{
    if (!s_initialized) return false;
    CmdPacket pkt;
    pkt.cmd        = CMD_MOVE_ABS;
    pkt.value_mm   = pos_mm;
    pkt.speed_mmps = (speed_mmps > 0) ? speed_mmps : g_settings.auto_speed_mmps;
    return esp_now_send(s_motor_mac, (uint8_t *)&pkt, sizeof(pkt)) == ESP_OK;
}

bool espnow_hmi_send_move_rel(float delta_mm, float speed_mmps)
{
    if (!s_initialized) return false;
    CmdPacket pkt;
    pkt.cmd        = CMD_MOVE_REL;
    pkt.value_mm   = delta_mm;
    pkt.speed_mmps = (speed_mmps > 0) ? speed_mmps : g_settings.jog_speed_mmps;
    return esp_now_send(s_motor_mac, (uint8_t *)&pkt, sizeof(pkt)) == ESP_OK;
}

bool espnow_hmi_send_home(void)
{
    if (!s_initialized) return false;
    uint8_t cmd = CMD_HOME;
    return esp_now_send(s_motor_mac, &cmd, 1) == ESP_OK;
}

bool espnow_hmi_send_stop(void)
{
    if (!s_initialized) return false;
    uint8_t cmd = CMD_STOP;
    return esp_now_send(s_motor_mac, &cmd, 1) == ESP_OK;
}

bool espnow_hmi_send_set_zero(void)
{
    if (!s_initialized) return false;
    uint8_t cmd = CMD_SET_ZERO;
    return esp_now_send(s_motor_mac, &cmd, 1) == ESP_OK;
}

bool espnow_hmi_send_clear_alarm(void)
{
    if (!s_initialized) return false;
    uint8_t cmd = CMD_CLEAR_ALARM;
    return esp_now_send(s_motor_mac, &cmd, 1) == ESP_OK;
}

bool espnow_hmi_send_restart(void)
{
    if (!s_initialized) return false;
    uint8_t cmd = CMD_RESTART;
    return esp_now_send(s_motor_mac, &cmd, 1) == ESP_OK;
}

bool espnow_hmi_send_sync(void)
{
    if (!s_initialized) return false;
    SyncPacket pkt = {};
    pkt.cmd             = CMD_SYNC;
    pkt.steps_per_mm    = g_settings.steps_per_mm;
    pkt.max_travel_mm   = g_settings.max_travel_mm;
    pkt.home_speed_mmps      = g_settings.home_speed_mmps;
    pkt.home_fast_speed_mmps = g_settings.home_fast_speed_mmps;
    pkt.home_odmak_mm        = g_settings.home_odmak_mm;
    pkt.home_clearance_mm    = g_settings.home_clearance_mm;
    pkt.auto_speed_mmps      = g_settings.auto_speed_mmps;
    pkt.accel_mmps2        = g_settings.accel_mmps2;
    pkt.home_offset_mm     = g_settings.home_offset_mm;
    pkt.overshoot_mm       = g_settings.overshoot_mm;
    pkt.approach_speed_mmps= g_settings.approach_speed_mmps;
    pkt.retract_mm         = g_settings.retract_mm;
    pkt.retract_speed_mmps = g_settings.retract_speed_mmps;
    pkt.retract_enabled    = (uint8_t)g_retract_enabled;
    pkt.direction          = (int8_t)g_settings.direction;
    return esp_now_send(s_motor_mac, (uint8_t *)&pkt, sizeof(pkt)) == ESP_OK;
}
