/*
 * CNC Back Gauge — Motor Brain
 * ESP32 WROOM-32 (30 pin)
 *
 * Hardware:
 *   GPIO18 = PUL-  → ZDM stepper drajver (16 mikrostep, P001=0016)
 *   GPIO19 = DIR-
 *   GPIO21 = ENA-  (INPUT_PULLUP, P108=0001 → drajver ignoriše)
 *   GPIO22 = HOME senzor (normally open tipkalo, INPUT_PULLUP)
 *
 * Mehanika:
 *   NEMA stepper 200 base steps × 16 mikrostep = 3200 spr
 *   Navoj: 5mm/okretaju → 640 koraka/mm
 *
 * Komunikacija:
 *   ESP-NOW ← HMI (ESP32-S3): CmdPacket / SyncPacket
 *   ESP-NOW → HMI (ESP32-S3): StatusPacket svakih 50ms
 *
 *   Motor MAC se ispisuje na Serial pri startu.
 *   HMI MAC se automatski nauči iz prvog primljenog paketa.
 */
#include <Arduino.h>
#include <WiFi.h>
#include "motor_ctrl.h"
#include "espnow_motor.h"

// ────────────────────────────────────────────────────────────────────────────
//  Status broadcast task
//  Šalje StatusPacket HMI-u svakih 50ms (20 Hz pozicija update)
// ────────────────────────────────────────────────────────────────────────────
static void status_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        motor_ctrl_limit_tick();          // home limit senzor
        motor_ctrl_driver_alarm_tick();   // drajver ALM signal
        motor_ctrl_bend_tick();           // bend senzor → auto retract
        espnow_motor_send_status();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(30));
    }
}

// ────────────────────────────────────────────────────────────────────────────
//  setup
// ────────────────────────────────────────────────────────────────────────────
void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("=== CNC Back Gauge — Motor Brain ===");

    // 1. Inicijalizuj motor (RMT + FreeRTOS stepper task na core 1)
    motor_ctrl_init();

    // 2. Inicijalizuj ESP-NOW (WiFi STA, bez AP veze)
    espnow_motor_init();

    // 3. Status task (core 0, niska prioriteta — ne ometa stepper na core 1)
    xTaskCreatePinnedToCore(
        status_task, "status_tx",
        3072, nullptr,
        2,    nullptr,
        0   // core 0
    );

    Serial.println("Motor Brain spreman za komande!");
    Serial.printf("[INFO] Ovaj MAC: %s\n", WiFi.macAddress().c_str());
    Serial.println("[INFO] Unesi ovaj MAC u espnow_hmi_set_motor_mac() na HMI strani");
    Serial.println("[INFO] ili ostavi broadcast (0xFF*6) za automatsko sparivanje.");
}

// ────────────────────────────────────────────────────────────────────────────
//  loop — sve radi u FreeRTOS taskovima, loop je slobodan za debug
// ────────────────────────────────────────────────────────────────────────────
void loop()
{
    // Svake sekunde ispiši status na Serial (za debug)
    static uint32_t last_print = 0;
    if (millis() - last_print >= 1000) {
        last_print = millis();
        const char *state_str[] = {"IDLE", "MOVING", "HOMING", "ALARM"};
        uint8_t st = motor_get_state();
        Serial.printf("[STATUS] pos=%.3f mm | state=%s | homed=%s | alarm=%d\n",
                      motor_get_position_mm(),
                      state_str[st < 4 ? st : 3],
                      motor_is_homed() ? "DA" : "NE",
                      (int)motor_get_alarm_code());
    }
    vTaskDelay(pdMS_TO_TICKS(100));
}
