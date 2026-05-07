// ====================================================================
//  espnow_motor.cpp — ESP-NOW primanje komandi + slanje statusa
//
//  recv callback se izvršava u WiFi tasku (core 0, high priority).
//  motor_ctrl operacije su thread-safe (StepperRMT koristi atomičke
//  varijable + FreeRTOS queue za komunikaciju sa stepper tasksom).
// ====================================================================
#include "espnow_motor.h"
#include "motor_ctrl.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <string.h>

// HMI MAC adresa (naučimo je iz prvog primljenog paketa)
static uint8_t s_hmi_mac[6]  = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static bool    s_hmi_known   = false;

// ── Primanje komandi od HMI ───────────────────────────────────────────────
static void on_recv(const esp_now_recv_info_t *info,
                    const uint8_t             *data,
                    int                        len)
{
    if (len < 1) return;

    // Zapamtimo HMI MAC adresu od prvog paketa (za slanje statusa)
    if (!s_hmi_known) {
        memcpy(s_hmi_mac, info->src_addr, 6);

        // Ukloni broadcast peer, dodaj unicast HMI peer
        uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
        esp_now_del_peer(broadcast);

        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, s_hmi_mac, 6);
        peer.channel = 0;
        peer.encrypt = false;
        esp_now_add_peer(&peer);
        s_hmi_known = true;

        Serial.printf("[ESP-NOW] HMI MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      s_hmi_mac[0], s_hmi_mac[1], s_hmi_mac[2],
                      s_hmi_mac[3], s_hmi_mac[4], s_hmi_mac[5]);
    }

    const uint8_t cmd = data[0];

    if (cmd == CMD_MOVE_ABS && len >= (int)sizeof(CmdPacket)) {
        const CmdPacket *pkt = (const CmdPacket *)data;
        Serial.printf("[CMD] MOVE_ABS %.3f mm @ %.1f mm/s\n",
                      pkt->value_mm, pkt->speed_mmps);
        motor_ctrl_move_abs(pkt->value_mm, pkt->speed_mmps);
    }
    else if (cmd == CMD_MOVE_REL && len >= (int)sizeof(CmdPacket)) {
        const CmdPacket *pkt = (const CmdPacket *)data;
        Serial.printf("[CMD] MOVE_REL %.3f mm @ %.1f mm/s\n",
                      pkt->value_mm, pkt->speed_mmps);
        motor_ctrl_move_rel(pkt->value_mm, pkt->speed_mmps);
    }
    else if (cmd == CMD_HOME) {
        Serial.println("[CMD] HOME");
        motor_ctrl_home();
    }
    else if (cmd == CMD_STOP) {
        Serial.println("[CMD] STOP");
        motor_ctrl_stop();
    }
    else if (cmd == CMD_SET_ZERO) {
        Serial.println("[CMD] SET_ZERO");
        motor_ctrl_set_zero();
    }
    else if (cmd == CMD_CLEAR_ALARM) {
        Serial.println("[CMD] CLEAR_ALARM");
        motor_ctrl_clear_alarm();
    }
    else if (cmd == CMD_RESTART) {
        Serial.println("[CMD] RESTART");
        vTaskDelay(pdMS_TO_TICKS(200));
        ESP.restart();
    }
    else if (cmd == CMD_SYNC && len >= (int)sizeof(SyncPacket)) {
        const SyncPacket *pkt = (const SyncPacket *)data;
        MotorConfig cfg = {};
        cfg.steps_per_mm    = pkt->steps_per_mm;
        cfg.max_travel_mm   = pkt->max_travel_mm;
        cfg.home_speed_mmps      = pkt->home_speed_mmps;
        cfg.home_fast_speed_mmps = pkt->home_fast_speed_mmps;
        cfg.home_odmak_mm        = pkt->home_odmak_mm;
        cfg.home_clearance_mm    = pkt->home_clearance_mm;
        cfg.auto_speed_mmps      = pkt->auto_speed_mmps;
        cfg.accel_mmps2        = pkt->accel_mmps2;
        cfg.home_offset_mm     = pkt->home_offset_mm;
        cfg.overshoot_mm        = pkt->overshoot_mm;
        cfg.approach_speed_mmps = pkt->approach_speed_mmps;
        cfg.retract_mm          = pkt->retract_mm;
        cfg.retract_speed_mmps  = pkt->retract_speed_mmps;
        cfg.direction           = pkt->direction;
        motor_ctrl_apply_config(cfg);
        motor_ctrl_set_retract_enabled((bool)pkt->retract_enabled);
        Serial.println("[CMD] SYNC primljen.");
    }
    else {
        Serial.printf("[ESP-NOW] Nepoznata komanda: 0x%02X len=%d\n", cmd, len);
    }
}

// ── Inicijalizacija ───────────────────────────────────────────────────────
void espnow_motor_init(void)
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    esp_err_t err = esp_now_init();
    if (err != ESP_OK) {
        Serial.printf("[ESP-NOW] Init greška: 0x%X\n", (unsigned)err);
        return;
    }

    esp_now_register_recv_cb(on_recv);

    // Broadcast peer za status slanje dok ne naučimo HMI MAC
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, s_hmi_mac, 6);   // 0xFF broadcast
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    Serial.println("[ESP-NOW] Motor spreman.");
    Serial.printf("[ESP-NOW] Motor MAC: %s\n", WiFi.macAddress().c_str());
    Serial.println("[ESP-NOW] Cekam komande od HMI...");
}

// ── Slanje statusa HMI-u ──────────────────────────────────────────────────
void espnow_motor_send_status(void)
{
    StatusPacket pkt = {};
    pkt.position_mm  = motor_get_position_mm();
    pkt.motor_state  = motor_get_state();
    pkt.is_homed     = (uint8_t)motor_is_homed();
    pkt.alarm_code   = motor_get_alarm_code();
    pkt.bend_sensor  = (uint8_t)motor_get_bend();

    esp_now_send(s_hmi_mac, (uint8_t *)&pkt, sizeof(pkt));
}
