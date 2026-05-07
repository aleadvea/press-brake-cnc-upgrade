/*
 * CNC Back Gauge - Waveshare ESP32-S3 7" LCD
 * Integracija svih ekrana sa navigacijom i storage-om
 */
#include <Arduino.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include "lvgl_v8_port.h"
#include "data_model.h"
#include "storage.h"
#include "ui_strings.h"
#include "ui_nav.h"
#include "ui_common.h"
#include "ui_manual.h"
#include "ui_auto.h"
#include "ui_programs.h"
#include "ui_materials.h"
#include "ui_settings.h"
#include "ui_alarms.h"
#include "espnow_hmi.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

// ------------------------------------------------------------------ //
//  Podaci za menu dugmad                                              //
// ------------------------------------------------------------------ //
static const char *MENU_ICONS[6] = {
    LV_SYMBOL_EDIT, LV_SYMBOL_LOOP, LV_SYMBOL_LIST,
    LV_SYMBOL_FILE, LV_SYMBOL_SETTINGS, LV_SYMBOL_WARNING,
};

static const char *menu_label(int idx)
{
    if (g_language == LANG_ENGLISH) {
        static const char *labels_en[6] = {
            "MANUAL MODE", "AUTO MODE", "PROGRAMS",
            "MATERIALS", "SETTINGS", "ALARMS"
        };
        return labels_en[idx];
    }

    static const char *labels_sr[6] = {
        "MANUELNI MOD", "AUTO MOD", "PROGRAMI",
        "MATERIJALI", "PODESAVANJA", "ALARMI"
    };
    return labels_sr[idx];
}
static const lv_color_t MENU_COLORS[6] = {
    LV_COLOR_MAKE(0x1E, 0x88, 0xE5),
    LV_COLOR_MAKE(0x43, 0xA0, 0x47),
    LV_COLOR_MAKE(0xFB, 0x8C, 0x00),
    LV_COLOR_MAKE(0x8E, 0x24, 0xAA),
    LV_COLOR_MAKE(0x54, 0x6E, 0x7A),
    LV_COLOR_MAKE(0xE5, 0x39, 0x35),
};
static const ScreenId MENU_SCREENS[6] = {
    SCREEN_MANUAL, SCREEN_AUTO, SCREEN_PROGRAMS,
    SCREEN_MATERIALS, SCREEN_SETTINGS, SCREEN_ALARMS,
};

static lv_obj_t *g_clock_label = nullptr;

static void clock_timer_cb(lv_timer_t *)
{
    if (!g_clock_label) return;
    uint32_t secs = millis() / 1000;
    char buf[12];
    lv_snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                (unsigned)(secs / 3600),
                (unsigned)((secs % 3600) / 60),
                (unsigned)(secs % 60));
    lv_label_set_text(g_clock_label, buf);
}

static void menu_btn_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    ui_nav_go(MENU_SCREENS[idx]);
}

// ------------------------------------------------------------------ //
//  Kreiranje glavnog menija (poziva ga ui_nav.cpp)                   //
// ------------------------------------------------------------------ //
void ui_main_menu_create(lv_obj_t *scr)
{
    g_clock_label = nullptr;
    lv_obj_set_style_bg_color(scr, LV_COLOR_MAKE(0x0D, 0x0F, 0x1A), 0);

    // Top bar
    lv_obj_t *topbar = lv_obj_create(scr);
    lv_obj_set_size(topbar, 800, 50);
    lv_obj_align(topbar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(topbar, LV_COLOR_MAKE(0x16, 0x1A, 0x2E), 0);
    lv_obj_set_style_border_side(topbar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(topbar, 2, 0);
    lv_obj_set_style_border_color(topbar, LV_COLOR_MAKE(0x2A, 0x3A, 0x5C), 0);
    lv_obj_set_style_radius(topbar, 0, 0);
    lv_obj_set_style_pad_all(topbar, 0, 0);
    lv_obj_clear_flag(topbar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(topbar);
    lv_label_set_text(title, LV_SYMBOL_HOME "  CNC BACK GAUGE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_MAKE(0xCF, 0xD8, 0xFF), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 14, 0);

    g_clock_label = lv_label_create(topbar);
    lv_label_set_text(g_clock_label, "00:00:00");
    lv_obj_set_style_text_font(g_clock_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g_clock_label, LV_COLOR_MAKE(0x82, 0xB1, 0xFF), 0);
    lv_obj_align(g_clock_label, LV_ALIGN_RIGHT_MID, -14, 0);
    lv_timer_t *clk_tmr = lv_timer_create(clock_timer_cb, 1000, nullptr);
    // Obrisi timer i ponisti pointer kad main-menu screen bude obrisan
    lv_obj_add_event_cb(scr, [](lv_event_t *e) {
        lv_timer_t *t = (lv_timer_t *)lv_event_get_user_data(e);
        if (t) lv_timer_del(t);
        g_clock_label = nullptr;
    }, LV_EVENT_DELETE, clk_tmr);
    clock_timer_cb(nullptr);

    // Grid dugmadi (3x2)
    lv_obj_t *grid = lv_obj_create(scr);
    lv_obj_set_size(grid, 800, 394);
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 10, 0);
    lv_obj_set_style_pad_row(grid, 10, 0);
    lv_obj_set_style_pad_column(grid, 10, 0);

    static lv_coord_t col_dsc[] = { LV_PCT(33), LV_PCT(33), LV_PCT(34), LV_GRID_TEMPLATE_LAST };
    static lv_coord_t row_dsc[] = { LV_PCT(50), LV_PCT(50), LV_GRID_TEMPLATE_LAST };
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

    for (int i = 0; i < 6; i++) {
        lv_obj_t *btn = lv_btn_create(grid);
        lv_obj_set_grid_cell(btn,
            LV_GRID_ALIGN_STRETCH, i % 3, 1,
            LV_GRID_ALIGN_STRETCH, i / 3, 1);
        lv_obj_set_style_bg_color(btn, MENU_COLORS[i], 0);
        lv_obj_set_style_bg_grad_color(btn, lv_color_darken(MENU_COLORS[i], LV_OPA_30), 0);
        lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_color(btn, lv_color_darken(MENU_COLORS[i], LV_OPA_20), LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, 14, 0);
        lv_obj_set_style_pad_all(btn, 10, 0);
        lv_obj_set_style_shadow_width(btn, 14, 0);
        lv_obj_set_style_shadow_color(btn, lv_color_darken(MENU_COLORS[i], LV_OPA_50), 0);
        lv_obj_set_style_shadow_ofs_y(btn, 5, 0);
        lv_obj_set_style_shadow_opa(btn, LV_OPA_50, 0);
        lv_obj_add_event_cb(btn, menu_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(btn, 8, 0);

        lv_obj_t *icon = lv_label_create(btn);
        lv_label_set_text(icon, MENU_ICONS[i]);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(icon, LV_COLOR_MAKE(0xFF, 0xFF, 0xFF), 0);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, menu_label(i));
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, LV_COLOR_MAKE(0xFF, 0xFF, 0xFF), 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(lbl, LV_PCT(100));
    }

    // Status bar
    lv_obj_t *status_bar = lv_obj_create(scr);
    lv_obj_set_size(status_bar, 800, 36);
    lv_obj_align(status_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(status_bar, LV_COLOR_MAKE(0x16, 0x1A, 0x2E), 0);
    lv_obj_set_style_border_side(status_bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(status_bar, 2, 0);
    lv_obj_set_style_border_color(status_bar, LV_COLOR_MAKE(0x2A, 0x3A, 0x5C), 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 4, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *slbl = lv_label_create(status_bar);
    lv_label_set_text(slbl,
        g_language == LANG_SERBIAN
            ? LV_SYMBOL_OK "  SPREMAN"
            : LV_SYMBOL_OK "  READY");
    lv_obj_set_style_text_font(slbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(slbl, LV_COLOR_MAKE(0x69, 0xF0, 0xAE), 0);
    lv_obj_center(slbl);
}

// ------------------------------------------------------------------ //
//  Startup dialog — izbor homing opcije pri svakom paljenju          //
// ------------------------------------------------------------------ //
static void ui_home_dialog_show(lv_timer_t *tmr)
{
    lv_timer_del(tmr);

    // Polu-transparentni overlay (blokira klikove ispod)
    lv_obj_t *overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(overlay, 800, 480);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_make(0x00, 0x00, 0x00), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

    // Dialog box 620x278 centriran
    lv_obj_t *box = lv_obj_create(overlay);
    lv_obj_set_size(box, 620, 278);
    lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(box, lv_color_make(0x0D, 0x0F, 0x1A), 0);
    lv_obj_set_style_border_color(box, lv_color_make(0xFF, 0xD7, 0x40), 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_radius(box, 14, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    // Naslov
    lv_obj_t *title = lv_label_create(box);
    lv_label_set_text(title,
        g_language == LANG_SERBIAN
            ? LV_SYMBOL_WARNING "  MOTOR NIJE HOMED"
            : LV_SYMBOL_WARNING "  MOTOR NOT HOMED");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_make(0xFF, 0xD7, 0x40), 0);
    lv_obj_set_pos(title, 0, 14);
    lv_obj_set_width(title, 620);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    // Podnaslov
    lv_obj_t *sub = lv_label_create(box);
    lv_label_set_text(sub,
        g_language == LANG_SERBIAN
            ? "Bez homing-a motor ne zna svoju poziciju."
            : "Without homing the motor position is unknown.");
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sub, lv_color_make(0x70, 0x80, 0xA0), 0);
    lv_obj_set_pos(sub, 0, 46);
    lv_obj_set_width(sub, 620);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);

    // Dugme 1: HOMING (fizicki senzor)
    lv_obj_t *btn1 = lv_btn_create(box);
    lv_obj_set_pos(btn1, 20, 76);
    lv_obj_set_size(btn1, 580, 54);
    lv_obj_set_style_bg_color(btn1, lv_color_make(0x4A, 0x14, 0x8C), 0);
    lv_obj_set_style_bg_color(btn1, lv_color_make(0x38, 0x0E, 0x6E), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn1, 10, 0);
    lv_obj_set_style_shadow_width(btn1, 0, 0);
    lv_obj_add_event_cb(btn1, [](lv_event_t *e) {
        espnow_hmi_send_home();
        lv_obj_del((lv_obj_t *)lv_event_get_user_data(e));
    }, LV_EVENT_CLICKED, overlay);
    lv_obj_t *l1 = lv_label_create(btn1);
    lv_label_set_text(l1,
        g_language == LANG_SERBIAN
            ? LV_SYMBOL_HOME "  Homing - trazi nultu tacku (treba senzor)"
            : LV_SYMBOL_HOME "  Homing - find zero point (sensor required)");
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l1, lv_color_white(), 0);
    lv_obj_center(l1);

    // Dugme 2: Postavi 0 ovde (bez senzora, za testiranje)
    lv_obj_t *btn2 = lv_btn_create(box);
    lv_obj_set_pos(btn2, 20, 138);
    lv_obj_set_size(btn2, 580, 54);
    lv_obj_set_style_bg_color(btn2, lv_color_make(0xBF, 0x46, 0x00), 0);
    lv_obj_set_style_bg_color(btn2, lv_color_make(0x9A, 0x38, 0x00), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn2, 10, 0);
    lv_obj_set_style_shadow_width(btn2, 0, 0);
    lv_obj_add_event_cb(btn2, [](lv_event_t *e) {
        espnow_hmi_send_set_zero();
        lv_obj_del((lv_obj_t *)lv_event_get_user_data(e));
    }, LV_EVENT_CLICKED, overlay);
    lv_obj_t *l2 = lv_label_create(btn2);
    lv_label_set_text(l2,
        g_language == LANG_SERBIAN
            ? LV_SYMBOL_GPS "  Postavi 0 ovde - bez senzora (testiranje)"
            : LV_SYMBOL_GPS "  Set 0 here - no sensor (testing)");
    lv_obj_set_style_text_font(l2, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l2, lv_color_white(), 0);
    lv_obj_center(l2);

    // Dugme 3: Nastavi bez homing-a
    lv_obj_t *btn3 = lv_btn_create(box);
    lv_obj_set_pos(btn3, 20, 206);
    lv_obj_set_size(btn3, 580, 42);
    lv_obj_set_style_bg_color(btn3, lv_color_make(0x1C, 0x28, 0x42), 0);
    lv_obj_set_style_bg_color(btn3, lv_color_make(0x16, 0x1E, 0x34), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn3, 10, 0);
    lv_obj_set_style_shadow_width(btn3, 0, 0);
    lv_obj_add_event_cb(btn3, [](lv_event_t *e) {
        lv_obj_del((lv_obj_t *)lv_event_get_user_data(e));
    }, LV_EVENT_CLICKED, overlay);
    lv_obj_t *l3 = lv_label_create(btn3);
    lv_label_set_text(l3,
        g_language == LANG_SERBIAN
            ? "Nastavi bez homing-a  (pozicija je nepoznata)"
            : "Continue without homing (position unknown)");
    lv_obj_set_style_text_font(l3, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l3, lv_color_make(0x70, 0x80, 0xA0), 0);
    lv_obj_center(l3);
}

// ------------------------------------------------------------------ //
//  setup / loop                                                       //
// ------------------------------------------------------------------ //
void setup()
{
    Serial.begin(115200);
    Serial.println("=== CNC Back Gauge HMI ===");

    storage_init();
    storage_load_all();

    // ESP-NOW mora biti inicijalizovan prije LVGL (WiFi stack)
    espnow_hmi_init();

    Serial.println("Inicijalizacija ekrana...");
    Board *board = new Board();
    board->init();
    assert(board->begin());
    Serial.println("Ekran OK.");

    Serial.println("Inicijalizacija LVGL...");
    assert(lvgl_port_init(board->getLCD(), board->getTouch()));
    Serial.println("LVGL OK.");

    Serial.println("Kreiranje UI...");
    lvgl_port_lock(-1);

    ui_nav_init();

    lv_obj_t *scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, LV_COLOR_MAKE(0x0D, 0x0F, 0x1A), 0);
    lv_obj_set_size(scr, 800, 480);
    ui_main_menu_create(scr);
    // 500ms timer: pošalji podesavanja motoru što ranije
    lv_timer_create([](lv_timer_t *t) {
        espnow_hmi_send_sync();
        lv_timer_del(t);
    }, 500, nullptr);

    // 1500ms timer: prikazi home dialog NAKON sto sync stigne do motora
    lv_timer_create(ui_home_dialog_show, 1500, nullptr);

    lv_scr_load(scr);

    ui_alarm_watcher_init();   // globalni popup kad alarm udari

    lvgl_port_unlock();
    Serial.println("UI spreman.");
}

void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}
