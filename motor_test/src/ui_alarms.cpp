#include "ui_alarms.h"
#include "ui_common.h"
#include "ui_nav.h"
#include "data_model.h"
#include "espnow_hmi.h"
#include "ui_strings.h"
#include <lvgl.h>
#include <cstdio>
#include <Arduino.h>

// ------------------------------------------------------------------ //
//  Status red (simulator dok nema ESP-NOW)                            //
// ------------------------------------------------------------------ //
struct StatusItem {
    const char *label;
    bool       *flag;
    bool        ok_is_true;  // true = zeleno kad true, false = zeleno kad false
    const char *ok_text;
    const char *err_text;
};

static StatusItem STATUSES[] = {
    { "MOTOR STATUS",      &g_machine.motor_ok,     true,  "OK",          "GRESKA"   },
    { "HOME SENZOR",       &g_machine.home_sensor,  true,  "AKTIVAN",     "NEAKTIVAN"},
    { "SENZOR SAVIJANJA",  &g_machine.bend_sensor,  false, "SLOBODAN",    "AKTIVAN"  },
    { "ALARM",             &g_machine.alarm_active, false, "NEMA ALARMA", "ALARM!"   },
    { "HOMING",            &g_machine.is_homed,     true,  "UREDJENO",    "POTREBNO" },
};
static const int STATUS_COUNT = sizeof(STATUSES) / sizeof(STATUSES[0]);

static lv_obj_t *s_status_rows[8];
static lv_obj_t *s_alarm_msg_lbl = nullptr;
static lv_obj_t *s_pos_lbl_alarm = nullptr;
static lv_timer_t *s_alarm_timer = nullptr;

static const char *status_label_text(int i)
{
    if (g_language == LANG_ENGLISH) {
        switch (i) {
            case 0: return "MOTOR STATUS";
            case 1: return "HOME SENSOR";
            case 2: return "BEND SENSOR";
            case 3: return "ALARM";
            case 4: return "HOMING";
            default: return "STATUS";
        }
    }
    return STATUSES[i].label;
}

static const char *status_value_text(int i, bool is_ok)
{
    if (g_language == LANG_ENGLISH) {
        switch (i) {
            case 0: return is_ok ? "OK" : "ERROR";
            case 1: return is_ok ? "ACTIVE" : "INACTIVE";
            case 2: return is_ok ? "FREE" : "ACTIVE";
            case 3: return is_ok ? "NO ALARM" : "ALARM!";
            case 4: return is_ok ? "DONE" : "REQUIRED";
            default: return is_ok ? "OK" : "ERROR";
        }
    }
    return is_ok ? STATUSES[i].ok_text : STATUSES[i].err_text;
}

static void update_alarm_display()
{
    for (int i = 0; i < STATUS_COUNT; i++) {
        if (!s_status_rows[i]) continue;
        lv_obj_t *row = s_status_rows[i];
        bool flagval = *STATUSES[i].flag;
        bool is_ok   = (flagval == STATUSES[i].ok_is_true);

        // Vrednost label (2. dijete)
        lv_obj_t *val = lv_obj_get_child(row, 1);
        if (val) {
            lv_label_set_text(val, status_value_text(i, is_ok));
            lv_obj_set_style_text_color(val,
                is_ok ? CLR_GREEN : CLR_RED, 0);
        }

        // Indikator krug (0. dijete)
        lv_obj_t *dot = lv_obj_get_child(row, 0);
        if (dot) {
            lv_obj_set_style_bg_color(dot,
                is_ok ? CLR_GREEN : CLR_RED, 0);
        }
    }

    if (s_alarm_msg_lbl) {
        lv_label_set_text(s_alarm_msg_lbl,
            g_machine.alarm_msg[0] ? g_machine.alarm_msg : (g_language == LANG_SERBIAN ? "—" : "—"));
    }
    if (s_pos_lbl_alarm) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.3f mm", g_machine.position_mm);
        lv_label_set_text(s_pos_lbl_alarm, buf);
    }
}

static void alarm_timer_cb(lv_timer_t *) {
    update_alarm_display();
}

static void reset_alarm_cb(lv_event_t *)
{
    // Pošalji CLEAR_ALARM motoru — motor brise alarm i postavlja s_homed=false
    espnow_hmi_send_clear_alarm();
    // Ocisti lokalni HMI prikaz
    g_machine.alarm_active = false;
    snprintf(g_machine.alarm_msg, sizeof(g_machine.alarm_msg),
             "%s", g_language == LANG_SERBIAN ? "Alarm obrisan. Obavezno uraditi HOMING!" : "Alarm cleared. HOMING required!");
    update_alarm_display();
    // Navigiraj na manual ekran gdje ce HOME dugme biti dostupno
    // (korisnik ce vidjeti status HOMING:POTREBNO u alarm ekranu)
}

static void restart_timer_cb(lv_timer_t *t)
{
    lv_timer_del(t);
    ESP.restart();
}

static void restart_both_cb(lv_event_t *)
{
    // 1. Šalji RESTART motoru (brain će se restartovati za ~200ms)
    espnow_hmi_send_restart();
    // 2. Restartuj HMI za 600ms (mora biti posle brain-a)
    lv_timer_create(restart_timer_cb, 600, nullptr);
}

// ---- Simulacioni toggle (za testiranje bez hardvera) ------------
static void toggle_motor_cb(lv_event_t *) {
    g_machine.motor_ok = !g_machine.motor_ok;
    update_alarm_display();
}
static void toggle_home_cb(lv_event_t *) {
    g_machine.home_sensor = !g_machine.home_sensor;
    update_alarm_display();
}
static void toggle_bend_cb(lv_event_t *) {
    g_machine.bend_sensor = !g_machine.bend_sensor;
    update_alarm_display();
}

void ui_alarms_create(lv_obj_t *parent)
{
    s_alarm_msg_lbl = nullptr;
    s_pos_lbl_alarm = nullptr;
    for (int i = 0; i < STATUS_COUNT; i++) s_status_rows[i] = nullptr;

    // ---- Status lista (levo, 470px) ---------------------------------
    lv_obj_t *stat_card = ui_card_create(parent, 8, CONTENT_Y + 8, 460, CONTENT_H - 10,
                                          g_language == LANG_SERBIAN ? "STATUS UREDJAJA" : "DEVICE STATUS");

    for (int i = 0; i < STATUS_COUNT; i++) {
        lv_obj_t *row = lv_obj_create(stat_card);
        s_status_rows[i] = row;
        lv_obj_set_size(row, 440, 52);
        lv_obj_set_pos(row, 8, 34 + i * 58);
        lv_obj_set_style_bg_color(row, CLR_TOPBAR, 0);
        lv_obj_set_style_border_color(row, CLR_BORDER, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_pad_hor(row, 12, 0);
        lv_obj_set_style_pad_ver(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        // Indikator krug
        lv_obj_t *dot = lv_obj_create(row);
        lv_obj_set_size(dot, 14, 14);
        lv_obj_align(dot, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, CLR_GREEN, 0);
        lv_obj_set_style_border_width(dot, 0, 0);

        // Vrednost
        lv_obj_t *val = lv_label_create(row);
        lv_label_set_text(val, STATUSES[i].ok_text);
        lv_obj_set_style_text_font(val, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(val, CLR_GREEN, 0);
        lv_obj_align(val, LV_ALIGN_RIGHT_MID, 0, 0);

        // Naziv
        lv_obj_t *nl = lv_label_create(row);
        lv_label_set_text(nl, status_label_text(i));
        lv_obj_set_style_text_font(nl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(nl, CLR_TEXT_DIM, 0);
        lv_obj_align(nl, LV_ALIGN_LEFT_MID, 22, 0);
    }

    // ---- Desna kolona: info + kontrole (320px) ----------------------
    lv_obj_t *right = ui_card_create(parent, 476, CONTENT_Y + 8, 316, CONTENT_H - 10,
                                      g_language == LANG_SERBIAN ? "INFO & KONTROLA" : "INFO & CONTROL");

    // Pozicija
    lv_obj_t *pl = lv_label_create(right);
    lv_label_set_text(pl, g_language == LANG_SERBIAN ? "POZICIJA:" : "POSITION:");
    lv_obj_set_style_text_font(pl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pl, CLR_TEXT_DIM, 0);
    lv_obj_set_pos(pl, 8, 36);

    s_pos_lbl_alarm = lv_label_create(right);
    lv_label_set_text(s_pos_lbl_alarm, "0.000 mm");
    lv_obj_set_style_text_font(s_pos_lbl_alarm, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_pos_lbl_alarm, CLR_ACCENT, 0);
    lv_obj_set_pos(s_pos_lbl_alarm, 8, 54);

    // Alarm poruka
    lv_obj_t *aml = lv_label_create(right);
    lv_label_set_text(aml, g_language == LANG_SERBIAN ? "ALARM PORUKA:" : "ALARM MESSAGE:");
    lv_obj_set_style_text_font(aml, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(aml, CLR_TEXT_DIM, 0);
    lv_obj_set_pos(aml, 8, 96);

    s_alarm_msg_lbl = lv_label_create(right);
    lv_label_set_text(s_alarm_msg_lbl, "—");
    lv_obj_set_style_text_font(s_alarm_msg_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_alarm_msg_lbl, CLR_RED, 0);
    lv_obj_set_pos(s_alarm_msg_lbl, 8, 114);

    // Reset alarm dugme
    lv_obj_t *rbtn = ui_btn_create(right,
                                    g_language == LANG_SERBIAN ? LV_SYMBOL_OK " RESET ALARMA" : LV_SYMBOL_OK " RESET ALARM",
                                    CLR_BTN_GO, 280, 40);
    lv_obj_set_pos(rbtn, 8, 154);
    lv_obj_add_event_cb(rbtn, reset_alarm_cb, LV_EVENT_CLICKED, nullptr);

    // Restart oba uredjaja
    lv_obj_t *restbtn = ui_btn_create(right,
                                       g_language == LANG_SERBIAN ? LV_SYMBOL_REFRESH " RESTART UREDJAJA" : LV_SYMBOL_REFRESH " RESTART DEVICE",
                                       lv_color_make(0x7B, 0x1F, 0x1F), 280, 40);
    lv_obj_set_pos(restbtn, 8, 202);
    lv_obj_add_event_cb(restbtn, restart_both_cb, LV_EVENT_CLICKED, nullptr);

    // ---- Simulacioni toggle (bez hardvera) - vidljivi kao "TEST" -----
    lv_obj_t *test_lbl = lv_label_create(right);
    lv_label_set_text(test_lbl, g_language == LANG_SERBIAN ? "TEST TOGGLE:" : "TEST TOGGLE:");
    lv_obj_set_style_text_font(test_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(test_lbl, CLR_TEXT_DIM, 0);
    lv_obj_set_pos(test_lbl, 8, 258);

    lv_obj_t *t1 = ui_btn_create(right, "MOTOR", lv_color_make(0x2A, 0x3A, 0x5C), 86, 34);
    lv_obj_set_pos(t1, 8, 276);
    lv_obj_add_event_cb(t1, toggle_motor_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *t2 = ui_btn_create(right, "HOME", lv_color_make(0x2A, 0x3A, 0x5C), 86, 34);
    lv_obj_set_pos(t2, 102, 276);
    lv_obj_add_event_cb(t2, toggle_home_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *t3 = ui_btn_create(right, "BEND", lv_color_make(0x2A, 0x3A, 0x5C), 86, 34);
    lv_obj_set_pos(t3, 196, 276);
    lv_obj_add_event_cb(t3, toggle_bend_cb, LV_EVENT_CLICKED, nullptr);

    // ---- LVGL timer osvezavanja (1s) --------------------------------
    s_alarm_timer = lv_timer_create(alarm_timer_cb, 1000, nullptr);

    // Obrisi timer i ponisti pointere kada screen bude obrisan
    lv_obj_add_event_cb(parent, [](lv_event_t *) {
        if (s_alarm_timer) {
            lv_timer_del(s_alarm_timer);
            s_alarm_timer = nullptr;
        }
        s_alarm_msg_lbl = nullptr;
        s_pos_lbl_alarm = nullptr;
        for (int i = 0; i < STATUS_COUNT; i++) s_status_rows[i] = nullptr;
    }, LV_EVENT_DELETE, nullptr);

    ui_statusbar_create(parent);
    update_alarm_display();
}
