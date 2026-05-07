/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
#include "lvgl_v8_port.h"

using namespace esp_panel::drivers;

#define LVGL_PORT_BUFFER_NUM_MAX    (2)

static SemaphoreHandle_t lvgl_mux         = nullptr;
static TaskHandle_t      lvgl_task_handle = nullptr;
static esp_timer_handle_t lvgl_tick_timer = nullptr;
static void *lvgl_buf[LVGL_PORT_BUFFER_NUM_MAX] = {};

// ------------------------------------------------------------------ //
//  Flush callback (simple, no avoid-tearing)                         //
// ------------------------------------------------------------------ //
static void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    LCD *lcd = (LCD *)drv->user_data;
    lcd->drawBitmap(area->x1, area->y1,
                    area->x2 - area->x1 + 1,
                    area->y2 - area->y1 + 1,
                    (const uint8_t *)color_map);
    // RGB panel: notify immediately (memory-mapped DMA)
    if (lcd->getBus()->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
        lv_disp_flush_ready(drv);
    }
}

static IRAM_ATTR bool onDrawBitmapFinishCallback(void *user_data)
{
    lv_disp_drv_t *drv = (lv_disp_drv_t *)user_data;
    lv_disp_flush_ready(drv);
    return false;
}

// ------------------------------------------------------------------ //
//  Touch input read callback                                          //
// ------------------------------------------------------------------ //
static SemaphoreHandle_t touch_detected;

static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    Touch *tp = (Touch *)indev_drv->user_data;
    TouchPoint point;
    data->state = LV_INDEV_STATE_RELEASED;

    if (tp->isInterruptEnabled() && (xSemaphoreTake(touch_detected, 0) == pdFALSE)) {
        return;
    }

    if (tp->readPoints(&point, 1, 0) > 0) {
        data->point.x = point.x;
        data->point.y = point.y;
        data->state   = LV_INDEV_STATE_PRESSED;
    }
}

static bool onTouchInterruptCallback(void *user_data)
{
    BaseType_t higher = pdFALSE;
    xSemaphoreGiveFromISR(touch_detected, &higher);
    portYIELD_FROM_ISR(higher);
    return false;
}

// ------------------------------------------------------------------ //
//  LVGL tick timer                                                    //
// ------------------------------------------------------------------ //
static void tick_increment(void *) { lv_tick_inc(LVGL_PORT_TICK_PERIOD_MS); }

static bool tick_init(void)
{
    const esp_timer_create_args_t args = { .callback = tick_increment, .name = "LVGL tick" };
    if (esp_timer_create(&args, &lvgl_tick_timer) != ESP_OK) return false;
    return esp_timer_start_periodic(lvgl_tick_timer, LVGL_PORT_TICK_PERIOD_MS * 1000) == ESP_OK;
}

// ------------------------------------------------------------------ //
//  LVGL task                                                          //
// ------------------------------------------------------------------ //
static void lvgl_port_task(void *)
{
    uint32_t delay_ms = LVGL_PORT_TASK_MAX_DELAY_MS;
    while (1) {
        if (lvgl_port_lock(-1)) {
            delay_ms = lv_timer_handler();
            lvgl_port_unlock();
        }
        delay_ms = (delay_ms > LVGL_PORT_TASK_MAX_DELAY_MS) ? LVGL_PORT_TASK_MAX_DELAY_MS
                 : (delay_ms < LVGL_PORT_TASK_MIN_DELAY_MS) ? LVGL_PORT_TASK_MIN_DELAY_MS
                 : delay_ms;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

// ------------------------------------------------------------------ //
//  Public API                                                         //
// ------------------------------------------------------------------ //
bool lvgl_port_init(LCD *lcd, Touch *tp)
{
    if (!lcd) return false;

    lv_init();
    if (!tick_init()) return false;

    // Alloc draw buffers
    int buf_size = lcd->getFrameWidth() * LVGL_PORT_BUFFER_SIZE_HEIGHT;
    for (int i = 0; i < LVGL_PORT_BUFFER_NUM && i < LVGL_PORT_BUFFER_NUM_MAX; i++) {
        lvgl_buf[i] = heap_caps_malloc(buf_size * sizeof(lv_color_t), LVGL_PORT_BUFFER_MALLOC_CAPS);
        assert(lvgl_buf[i]);
    }

    // Register display driver
    static lv_disp_draw_buf_t disp_buf;
    static lv_disp_drv_t      disp_drv;
    lv_disp_draw_buf_init(&disp_buf, lvgl_buf[0], lvgl_buf[1], buf_size);
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb  = flush_callback;
    disp_drv.hor_res   = lcd->getFrameWidth();
    disp_drv.ver_res   = lcd->getFrameHeight();
    disp_drv.draw_buf  = &disp_buf;
    disp_drv.user_data = (void *)lcd;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
    if (!disp) return false;

    // For non-RGB LCD, attach finish callback
    if (lcd->getBus()->getBasicAttributes().type != ESP_PANEL_BUS_TYPE_RGB) {
        lcd->attachDrawBitmapFinishCallback(onDrawBitmapFinishCallback, (void *)disp->driver);
    }

    // Register touch driver
    if (tp) {
        static lv_indev_drv_t indev_drv;
        if (tp->isInterruptEnabled()) {
            touch_detected = xSemaphoreCreateBinary();
            tp->attachInterruptCallback(onTouchInterruptCallback, tp);
        }
        lv_indev_drv_init(&indev_drv);
        indev_drv.type      = LV_INDEV_TYPE_POINTER;
        indev_drv.read_cb   = touchpad_read;
        indev_drv.user_data = (void *)tp;
        lv_indev_drv_register(&indev_drv);
    }

    // Create mutex and LVGL task
    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    if (!lvgl_mux) return false;

    BaseType_t core = (LVGL_PORT_TASK_CORE < 0) ? tskNO_AFFINITY : LVGL_PORT_TASK_CORE;
    if (xTaskCreatePinnedToCore(lvgl_port_task, "lvgl",
                                LVGL_PORT_TASK_STACK_SIZE, nullptr,
                                LVGL_PORT_TASK_PRIORITY, &lvgl_task_handle, core) != pdPASS) {
        return false;
    }

    return true;
}

bool lvgl_port_lock(int timeout_ms)
{
    if (!lvgl_mux) return false;
    TickType_t ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, ticks) == pdTRUE;
}

bool lvgl_port_unlock(void)
{
    if (!lvgl_mux) return false;
    xSemaphoreGiveRecursive(lvgl_mux);
    return true;
}

bool lvgl_port_deinit(void)
{
    if (lvgl_tick_timer) {
        esp_timer_stop(lvgl_tick_timer);
        esp_timer_delete(lvgl_tick_timer);
    }
    if (lvgl_port_lock(-1)) {
        if (lvgl_task_handle) { vTaskDelete(lvgl_task_handle); lvgl_task_handle = nullptr; }
        lvgl_port_unlock();
    }
    for (int i = 0; i < LVGL_PORT_BUFFER_NUM_MAX; i++) {
        if (lvgl_buf[i]) { free(lvgl_buf[i]); lvgl_buf[i] = nullptr; }
    }
    if (lvgl_mux) { vSemaphoreDelete(lvgl_mux); lvgl_mux = nullptr; }
    return true;
}
