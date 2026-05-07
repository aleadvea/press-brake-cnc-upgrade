/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "sdkconfig.h"
#ifdef CONFIG_ARDUINO_RUNNING_CORE
#include <Arduino.h>
#endif
#include "esp_display_panel.hpp"
#include "lvgl.h"

// *INDENT-OFF*

#define LVGL_PORT_TICK_PERIOD_MS            (2)

// Buffer allocated in SRAM (faster for small buffers)
#define LVGL_PORT_BUFFER_MALLOC_CAPS        (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#define LVGL_PORT_BUFFER_SIZE_HEIGHT        (20)
#define LVGL_PORT_BUFFER_NUM                (2)

#define LVGL_PORT_TASK_MAX_DELAY_MS         (500)
#define LVGL_PORT_TASK_MIN_DELAY_MS         (2)
#define LVGL_PORT_TASK_STACK_SIZE           (6 * 1024)
#define LVGL_PORT_TASK_PRIORITY             (2)
#ifdef ARDUINO_RUNNING_CORE
#define LVGL_PORT_TASK_CORE                 (ARDUINO_RUNNING_CORE)
#else
#define LVGL_PORT_TASK_CORE                 (0)
#endif

// Avoid tearing: 0=disable, 1=LCD double-buf+full-refresh,
//                2=LCD triple-buf+full-refresh, 3=LCD double-buf+direct (recommended)
#define LVGL_PORT_AVOID_TEARING_MODE        (0)
#define LVGL_PORT_ROTATION_DEGREE           (0)

// *INDENT-ON*

#ifdef __cplusplus
extern "C" {
#endif

bool lvgl_port_init(esp_panel::drivers::LCD *lcd, esp_panel::drivers::Touch *tp);
bool lvgl_port_deinit(void);
bool lvgl_port_lock(int timeout_ms);
bool lvgl_port_unlock(void);

#ifdef __cplusplus
}
#endif
