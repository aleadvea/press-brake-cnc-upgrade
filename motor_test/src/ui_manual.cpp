#include "ui_manual.h"
#include "ui_common.h"
#include "data_model.h"
#include "espnow_hmi.h"
#include "ui_strings.h"
#include <lvgl.h>
#include <cstdio>
#include <cstring>

// ------------------------------------------------------------------ //
//  Konstante                                                          //
// ------------------------------------------------------------------ //
static float        INCREMENTS[]  = { 0.05f, 0.01f, 1.0f, 10.0f, 50.0f };
static const int    INC_COUNT     = 5;

// ------------------------------------------------------------------ //
//  State                                                              //
// ------------------------------------------------------------------ //
static int          s_inc_idx     = 2;
static float        s_cur_mm      = 0.0f;
static float        s_target_mm   = 0.0f;

static lv_obj_t    *s_cur_label   = nullptr;
static lv_obj_t    *s_tgt_label   = nullptr;
static lv_obj_t    *s_mac_label   = nullptr;  // pozicija bez offseta
static lv_obj_t    *s_inc_btns[INC_COUNT] = {};
static lv_obj_t    *s_go_btn      = nullptr;
static lv_obj_t    *s_udalji_btn  = nullptr;
static lv_obj_t    *s_priblizi_btn = nullptr;
static lv_obj_t    *s_statusbar   = nullptr;
static lv_timer_t  *s_pos_timer   = nullptr;
static uint32_t     s_inc_press_start[INC_COUNT] = {};
static bool         s_inc_long_fired[INC_COUNT]  = {};

// ------------------------------------------------------------------ //
//  Layout constants (800 x 480, topbar=50, statusbar=36)             //
//  Content area: y=50 .. y=444  (394px)                              //
//                                                                     //
//  y=54   POS/TGT cards  h=110                                       //
//  y=172  INC row        h=48                                        //
//  y=228  NAZAD/NAPRED   h=84                                        //
//  y=320  IDI NA / POKRENI / STOP  h=52                              //
//  y=380  (free 64px)                                                //
//  y=444  statusbar                                                   //
// ------------------------------------------------------------------ //

// ------------------------------------------------------------------ //
//  Helpers                                                            //
// ------------------------------------------------------------------ //
// Forward deklaracija (pos_sync_cb je definisan prije update_display)
static void update_display();

// Timer callback: sinhronizuj prikaz pozicije iz g_machine (popunjava ESP-NOW recv_cb)
static void pos_sync_cb(lv_timer_t *)
{
    static bool s_was_alarm  = false;
    static bool s_was_homing = false;
    bool in_alarm  = (g_machine.motor_state == MSTATE_ALARM);
    bool in_homing = (g_machine.motor_state == MSTATE_HOMING);

    // Kad alarm nestane — sinhroniziraj target sa stvarnom pozicijom
    if (s_was_alarm && !in_alarm) {
        s_target_mm = g_machine.position_mm;
    }
    // Kad homing završi — sinhroniziraj target na finalni odmak od senzora
    if (s_was_homing && !in_homing && g_machine.is_homed) {
        s_target_mm = g_machine.position_mm;
    }
    s_was_alarm  = in_alarm;
    s_was_homing = in_homing;

    s_cur_mm = g_machine.position_mm;
    // Statusbar: jasno pokaži stanje motora
    if (s_statusbar) {
        uint8_t st = g_machine.motor_state;
        if      (st == MSTATE_ALARM)
            ui_statusbar_set(s_statusbar,
                g_language == LANG_SERBIAN
                    ? LV_SYMBOL_WARNING "  GRESKA! Pritisni RESET ALARM"
                    : LV_SYMBOL_WARNING "  ERROR! Press RESET ALARM",
                CLR_RED);
        else if (!g_machine.is_homed)
            ui_statusbar_set(s_statusbar,
                g_language == LANG_SERBIAN
                    ? LV_SYMBOL_WARNING "  Nije referiran - pritisni HOMING"
                    : LV_SYMBOL_WARNING "  Not homed - press HOMING",
                CLR_YELLOW);
        else if (st == MSTATE_HOMING)
            ui_statusbar_set(s_statusbar,
                g_language == LANG_SERBIAN
                    ? LV_SYMBOL_HOME "  Homing u toku..."
                    : LV_SYMBOL_HOME "  Homing in progress...",
                CLR_YELLOW);
        else if (st == MSTATE_MOVING)
            ui_statusbar_set(s_statusbar,
                g_language == LANG_SERBIAN
                    ? LV_SYMBOL_PLAY "  Kretanje..."
                    : LV_SYMBOL_PLAY "  Moving...",
                CLR_ACCENT);
        else
            ui_statusbar_set(s_statusbar,
                g_language == LANG_SERBIAN
                    ? LV_SYMBOL_OK "  Spreman"
                    : LV_SYMBOL_OK "  Ready",
                CLR_GREEN);
    }
    update_display();
}

static void update_display()
{
    char buf[32];

    if (s_cur_label) {
        snprintf(buf, sizeof(buf), "%.3f", s_cur_mm);
        lv_label_set_text(s_cur_label, buf);
    }
    if (s_mac_label) {
        float raw = s_cur_mm - g_settings.home_offset_mm;
        snprintf(buf, sizeof(buf), "%.2f", raw);
        lv_label_set_text(s_mac_label, buf);
    }

    if (s_tgt_label) {
        snprintf(buf, sizeof(buf), "%.3f", s_target_mm);
        lv_label_set_text(s_tgt_label, buf);
        lv_obj_set_style_text_color(s_tgt_label,
            s_target_mm != s_cur_mm ? CLR_YELLOW : CLR_ACCENT, 0);
    }

    bool in_alarm = (g_machine.motor_state == MSTATE_ALARM);
    float min_mm  = g_settings.home_offset_mm - g_settings.max_travel_mm;
    bool at_min   = (s_cur_mm <= min_mm + 0.5f);

    if (s_go_btn) {
        bool can = (s_target_mm != s_cur_mm) && !in_alarm && g_machine.is_homed;
        lv_obj_set_style_bg_color(s_go_btn,
            can ? CLR_BTN_GO : lv_color_make(0x22, 0x2C, 0x44), 0);
        if (can) lv_obj_add_flag(s_go_btn,   LV_OBJ_FLAG_CLICKABLE);
        else     lv_obj_clear_flag(s_go_btn, LV_OBJ_FLAG_CLICKABLE);
    }

    // UDALJI onemogucen tokom alarma i kad smo na safe_max (uz senzor)
    float safe_max = g_settings.home_offset_mm - g_settings.home_clearance_mm;
    bool at_max = (s_cur_mm >= safe_max - 0.5f);
    if (s_udalji_btn) {
        bool disable_uda = in_alarm || at_max;
        lv_obj_set_style_bg_color(s_udalji_btn,
            disable_uda ? lv_color_make(0x22, 0x22, 0x22) : lv_color_make(0x1A, 0x3A, 0x6E), 0);
        if (disable_uda) lv_obj_clear_flag(s_udalji_btn, LV_OBJ_FLAG_CLICKABLE);
        else             lv_obj_add_flag(s_udalji_btn,   LV_OBJ_FLAG_CLICKABLE);
    }

    // PRIBLIZI onemogucen na poziciji 0 (ne moze ispod nule)
    if (s_priblizi_btn) {
        lv_obj_set_style_bg_color(s_priblizi_btn,
            at_min ? lv_color_make(0x22, 0x22, 0x22) : lv_color_make(0x1B, 0x5E, 0x20), 0);
        if (at_min) lv_obj_clear_flag(s_priblizi_btn, LV_OBJ_FLAG_CLICKABLE);
        else        lv_obj_add_flag(s_priblizi_btn,   LV_OBJ_FLAG_CLICKABLE);
    }
}

static void select_increment(int idx)
{
    s_inc_idx = idx;
    for (int i = 0; i < INC_COUNT; i++) {
        if (!s_inc_btns[i]) continue;
        lv_obj_set_style_bg_color(s_inc_btns[i],
            i == idx ? lv_color_make(0x1E, 0x88, 0xE5)
                     : lv_color_make(0x1C, 0x28, 0x42), 0);
        lv_obj_set_style_border_color(s_inc_btns[i],
            i == idx ? lv_color_make(0x1E, 0x88, 0xE5)
                     : CLR_BORDER, 0);
    }
}

static void inc_label_text(float v, char *out, size_t out_sz)
{
    snprintf(out, out_sz, "%.3f", v);
    char *dot = strchr(out, '.');
    if (!dot) return;
    char *end = out + strlen(out) - 1;
    while (end > dot && *end == '0') *end-- = '\0';
    if (*end == '.') *end = '\0';
}

static void refresh_increment_labels()
{
    for (int i = 0; i < INC_COUNT; i++) {
        if (!s_inc_btns[i]) continue;
        lv_obj_t *lbl = lv_obj_get_child(s_inc_btns[i], 0);
        if (!lbl) continue;
        char num[16];
        char txt[24];
        inc_label_text(INCREMENTS[i], num, sizeof(num));
        snprintf(txt, sizeof(txt), "%s mm", num);
        lv_label_set_text(lbl, txt);
    }
}

static void inc_edit_done(float val, void *ud)
{
    int idx = (int)(intptr_t)ud;
    if (idx < 0 || idx >= INC_COUNT) return;
    if (val < 0.001f) val = 0.001f;
    if (val > 100.0f) val = 100.0f;
    INCREMENTS[idx] = val;
    refresh_increment_labels();
    select_increment(idx);
    if (s_statusbar) {
        ui_statusbar_set(s_statusbar,
            g_language == LANG_SERBIAN
                ? LV_SYMBOL_EDIT "  JOG KORAK IZMENJEN"
                : LV_SYMBOL_EDIT "  JOG STEP UPDATED",
            CLR_YELLOW);
    }
}

// ------------------------------------------------------------------ //
//  Callbacks                                                          //
// ------------------------------------------------------------------ //
static void inc_btn_cb(lv_event_t *e)
{
    select_increment((int)(intptr_t)lv_event_get_user_data(e));
}

static void inc_press_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= INC_COUNT) return;
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        s_inc_press_start[idx] = lv_tick_get();
        s_inc_long_fired[idx] = false;
        return;
    }
    if (code == LV_EVENT_PRESSING) {
        if (!s_inc_long_fired[idx] && lv_tick_elaps(s_inc_press_start[idx]) >= 3000) {
            s_inc_long_fired[idx] = true;
            ui_numpad_open(g_language == LANG_SERBIAN ? "JOG KORAK (mm)" : "JOG STEP (mm)", INCREMENTS[idx],
                           inc_edit_done, (void *)(intptr_t)idx);
        }
        return;
    }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        s_inc_long_fired[idx] = false;
    }
}

static void udalji_cb(lv_event_t *)
{
    float safe_max = g_settings.home_offset_mm - g_settings.home_clearance_mm;
    s_target_mm += INCREMENTS[s_inc_idx];
    if (s_target_mm > safe_max) s_target_mm = safe_max;
    espnow_hmi_send_move_rel(INCREMENTS[s_inc_idx], 0);
    update_display();
    if (s_statusbar)
        ui_statusbar_set(s_statusbar,
            g_language == LANG_SERBIAN
                ? LV_SYMBOL_LEFT "  JOG UDALJI"
                : LV_SYMBOL_LEFT "  JOG AWAY",
            CLR_ACCENT);
}

static void priblizi_cb(lv_event_t *)
{
    s_target_mm -= INCREMENTS[s_inc_idx];
    if (s_target_mm < 0) s_target_mm = 0;
    espnow_hmi_send_move_rel(-INCREMENTS[s_inc_idx], 0);
    update_display();
    if (s_statusbar)
        ui_statusbar_set(s_statusbar,
            g_language == LANG_SERBIAN
                ? LV_SYMBOL_RIGHT "  JOG PRIBLIZI"
                : LV_SYMBOL_RIGHT "  JOG CLOSER",
            CLR_ACCENT);
}

static void goto_numpad_done(float val, void *)
{
    float safe_max = g_settings.home_offset_mm - g_settings.home_clearance_mm;
    float min_mm   = g_settings.home_offset_mm - g_settings.max_travel_mm;
    if (val < min_mm)   val = min_mm;
    if (val > safe_max) val = safe_max;
    s_target_mm = val;
    update_display();
    if (s_statusbar)
        ui_statusbar_set(s_statusbar,
            g_language == LANG_SERBIAN
                ? LV_SYMBOL_EDIT "  TARGET POSTAVLJEN - pritisni POKRENI"
                : LV_SYMBOL_EDIT "  TARGET SET - press START",
            CLR_YELLOW);
}

static void goto_cb(lv_event_t *)
{
    ui_numpad_open_empty(g_language == LANG_SERBIAN ? "IDI NA POZICIJU (mm)" : "GO TO POSITION (mm)",
                         goto_numpad_done, nullptr);
}

static void execute_cb(lv_event_t *)
{
    espnow_hmi_send_move_abs(s_target_mm, 0);
    update_display();
    if (s_statusbar)
        ui_statusbar_set(s_statusbar,
            g_language == LANG_SERBIAN
                ? LV_SYMBOL_PLAY "  POKRENUTO - cekam motor..."
                : LV_SYMBOL_PLAY "  STARTED - waiting for motor...",
            CLR_GREEN);
}

static void stop_cb(lv_event_t *)
{
    espnow_hmi_send_stop();
    s_target_mm = s_cur_mm;
    update_display();
    if (s_statusbar)
        ui_statusbar_set(s_statusbar,
            g_language == LANG_SERBIAN
                ? LV_SYMBOL_STOP "  STOP POSLAN"
                : LV_SYMBOL_STOP "  STOP SENT",
            CLR_RED);
}

static void home_cb(lv_event_t *)
{
    // Msgbox sa potvrdom
    static const char *btns_sr[] = { "DA", "NE", "" };
    static const char *btns_en[] = { "YES", "NO", "" };
    lv_obj_t *mbox = lv_msgbox_create(lv_scr_act(), "HOMING",
        g_language == LANG_SERBIAN
            ? LV_SYMBOL_HOME "  Pokreni proceduru 0 (homing)?\nMotor ce se kretati ka senzoru!"
            : LV_SYMBOL_HOME "  Start homing procedure?\nMotor will move toward sensor!",
        g_language == LANG_SERBIAN ? btns_sr : btns_en, false);
    lv_obj_set_style_width(mbox, 440, 0);
    lv_obj_center(mbox);
    lv_obj_add_event_cb(mbox, [](lv_event_t *e) {
        lv_obj_t *mb  = (lv_obj_t *)lv_event_get_current_target(e);
        const char *btn = lv_msgbox_get_active_btn_text(mb);
        if (btn && ((g_language == LANG_SERBIAN && btn[0] == 'D') || (g_language == LANG_ENGLISH && btn[0] == 'Y'))) {
            espnow_hmi_send_home();
            if (s_statusbar)
                ui_statusbar_set(s_statusbar,
                    LV_SYMBOL_HOME "  HOMING...", CLR_YELLOW);
        }
        lv_msgbox_close(mb);
    }, LV_EVENT_VALUE_CHANGED, nullptr);
}

// ------------------------------------------------------------------ //
//  Helper: styled card container                                      //
// ------------------------------------------------------------------ //
static lv_obj_t *make_card(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, w, h);
    lv_obj_set_style_bg_color(c, lv_color_make(0x10, 0x18, 0x2C), 0);
    lv_obj_set_style_border_color(c, CLR_BORDER, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_radius(c, 10, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

// ------------------------------------------------------------------ //
//  Kreiranje ekrana                                                   //
// ------------------------------------------------------------------ //
void ui_manual_create(lv_obj_t *parent)
{
    s_cur_label = s_tgt_label = s_go_btn = s_udalji_btn = s_priblizi_btn = s_statusbar = nullptr;
    for (int i = 0; i < INC_COUNT; i++) s_inc_btns[i] = nullptr;
    s_cur_mm    = g_machine.position_mm;
    s_target_mm = s_cur_mm;

    const int LX  = 8;     // leva kolona x
    const int LW  = 545;   // leva kolona sirina
    const int RX  = 561;   // desna kolona x
    const int RW  = 231;   // desna kolona sirina (uza)
    const int SY  = CONTENT_Y + 6;  // start y = 56
    const int RBTN_H = (CONTENT_H - 20) / 2;  // visina UDALJI/PRIBLIZI dugmadi

    // ==============================================================
    // DESNA KOLONA: UDALJI (gore) / PRIBLIZI (dole)
    // ==============================================================
    lv_obj_t *bnaz = lv_btn_create(parent);
    s_udalji_btn = bnaz;
    lv_obj_set_pos(bnaz, RX, SY);
    lv_obj_set_size(bnaz, RW, RBTN_H);
    lv_obj_set_style_bg_color(bnaz, lv_color_make(0x1A, 0x3A, 0x6E), 0);
    lv_obj_set_style_bg_color(bnaz, lv_color_make(0x15, 0x2C, 0x58), LV_STATE_PRESSED);
    lv_obj_set_style_radius(bnaz, 12, 0);
    lv_obj_set_style_shadow_width(bnaz, 0, 0);
    lv_obj_add_event_cb(bnaz, udalji_cb, LV_EVENT_CLICKED, nullptr);
    { lv_obj_t *ic = lv_label_create(bnaz);
      lv_label_set_text(ic, LV_SYMBOL_UP);
      lv_obj_set_style_text_font(ic, &lv_font_montserrat_28, 0);
      lv_obj_set_style_text_color(ic, lv_color_white(), 0);
      lv_obj_align(ic, LV_ALIGN_CENTER, 0, -14); }
    { lv_obj_t *tx = lv_label_create(bnaz);
            lv_label_set_text(tx, g_language == LANG_SERBIAN ? "UDALJI" : "MOVE AWAY");
      lv_obj_set_style_text_font(tx, &lv_font_montserrat_20, 0);
      lv_obj_set_style_text_color(tx, lv_color_white(), 0);
      lv_obj_align(tx, LV_ALIGN_CENTER, 0, 18); }

    lv_obj_t *bnap = lv_btn_create(parent);
    s_priblizi_btn = bnap;
    lv_obj_set_pos(bnap, RX, SY + RBTN_H + 8);
    lv_obj_set_size(bnap, RW, RBTN_H);
    lv_obj_set_style_bg_color(bnap, lv_color_make(0x1B, 0x5E, 0x20), 0);
    lv_obj_set_style_bg_color(bnap, lv_color_make(0x15, 0x4A, 0x18), LV_STATE_PRESSED);
    lv_obj_set_style_radius(bnap, 12, 0);
    lv_obj_set_style_shadow_width(bnap, 0, 0);
    lv_obj_add_event_cb(bnap, priblizi_cb, LV_EVENT_CLICKED, nullptr);
    { lv_obj_t *tx = lv_label_create(bnap);
            lv_label_set_text(tx, g_language == LANG_SERBIAN ? "PRIBLIZI" : "MOVE CLOSER");
      lv_obj_set_style_text_font(tx, &lv_font_montserrat_20, 0);
      lv_obj_set_style_text_color(tx, lv_color_white(), 0);
      lv_obj_align(tx, LV_ALIGN_CENTER, 0, -18); }
    { lv_obj_t *ic = lv_label_create(bnap);
      lv_label_set_text(ic, LV_SYMBOL_DOWN);
      lv_obj_set_style_text_font(ic, &lv_font_montserrat_28, 0);
      lv_obj_set_style_text_color(ic, lv_color_white(), 0);
      lv_obj_align(ic, LV_ALIGN_CENTER, 0, 14); }

    // ==============================================================
    // LEVA KOLONA
    // ==============================================================
    // Layout (content y=50..444, h=394):
    //  pcard  y=56  h=160   → end 216
    //  INC    y=224 h=46    → end 270
    //  ACTION y=278 h=56    → end 334
    //  HOME   y=370 h=56    → end 426  (spušteno dole)
    const int ACT_Y  = SY + 160 + 8;   // 224
    const int HOME_Y = CONTENT_Y + CONTENT_H - 62;  // pri dnu (≈382)
    int y = SY;

    // 1) Pozicija kartica — POZICIJA lijevo, TARGET + MACHINE desno (jedno ispod drugog)
    lv_obj_t *pcard = lv_obj_create(parent);
    lv_obj_set_pos(pcard, LX, y);
    lv_obj_set_size(pcard, LW, 160);
    lv_obj_set_style_bg_color(pcard, lv_color_make(0x10, 0x18, 0x2C), 0);
    lv_obj_set_style_border_color(pcard, CLR_BORDER, 0);
    lv_obj_set_style_border_width(pcard, 1, 0);
    lv_obj_set_style_radius(pcard, 10, 0);
    lv_obj_set_style_pad_all(pcard, 0, 0);
    lv_obj_clear_flag(pcard, LV_OBJ_FLAG_SCROLLABLE);

    // --- Lijevo: POZICIJA (font 48) ---
    { lv_obj_t *l = lv_label_create(pcard);
            lv_label_set_text(l, g_language == LANG_SERBIAN ? LV_SYMBOL_GPS "  POZICIJA" : LV_SYMBOL_GPS "  POSITION");
      lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(l, CLR_TEXT_DIM, 0);
      lv_obj_set_pos(l, 12, 8); }
    s_cur_label = lv_label_create(pcard);
    lv_obj_set_style_text_font(s_cur_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_cur_label, CLR_GREEN, 0);
    lv_obj_set_pos(s_cur_label, 12, 28);
    { lv_obj_t *l = lv_label_create(pcard);
      lv_label_set_text(l, "mm");
      lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_color(l, CLR_TEXT_DIM, 0);
      lv_obj_set_pos(l, 14, 120); }

    // --- Desno gore: TARGET ---
    { lv_obj_t *l = lv_label_create(pcard);
      lv_label_set_text(l, LV_SYMBOL_RIGHT "  TARGET");
      lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(l, CLR_TEXT_DIM, 0);
      lv_obj_set_pos(l, 340, 10); }
    s_tgt_label = lv_label_create(pcard);
    lv_obj_set_style_text_font(s_tgt_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_tgt_label, CLR_ACCENT, 0);
    lv_obj_set_pos(s_tgt_label, 340, 26);
    { lv_obj_t *l = lv_label_create(pcard);
      lv_label_set_text(l, "mm");
      lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(l, CLR_TEXT_DIM, 0);
      lv_obj_set_pos(l, 340, 58); }

    // --- Desno dole: MACHINE ---
    { lv_obj_t *l = lv_label_create(pcard);
      lv_label_set_text(l, LV_SYMBOL_RIGHT "  MACHINE");
      lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(l, CLR_TEXT_DIM, 0);
      lv_obj_set_pos(l, 340, 82); }
    s_mac_label = lv_label_create(pcard);
    lv_obj_set_style_text_font(s_mac_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_mac_label, lv_color_make(0xA0, 0xC0, 0xFF), 0);
    lv_obj_set_pos(s_mac_label, 340, 96);
    { lv_obj_t *l = lv_label_create(pcard);
      lv_label_set_text(l, "mm");
      lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(l, CLR_TEXT_DIM, 0);
      lv_obj_set_pos(l, 340, 130); }
    y += 160 + 8;

    // 2) Inkrement dugmad
    const int INC_BW = (LW - 4*4) / 5;
    for (int i = 0; i < INC_COUNT; i++) {
        lv_obj_t *b = lv_btn_create(parent);
        s_inc_btns[i] = b;
        lv_obj_set_pos(b, LX + i * (INC_BW + 4), y);
        lv_obj_set_size(b, INC_BW, 46);
        lv_obj_set_style_radius(b, 8, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_add_event_cb(b, inc_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, inc_press_cb, LV_EVENT_PRESSED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, inc_press_cb, LV_EVENT_PRESSING, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, inc_press_cb, LV_EVENT_RELEASED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, inc_press_cb, LV_EVENT_PRESS_LOST, (void *)(intptr_t)i);
        char nbuf[16];
        char lbl[24];
        inc_label_text(INCREMENTS[i], nbuf, sizeof(nbuf));
        snprintf(lbl, sizeof(lbl), "%s mm", nbuf);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, lbl);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);
        lv_obj_center(l);
    }
    y += 46 + 8;

    // 3) IDI NA + POKRENI + STOP
    lv_obj_t *bgoto = lv_btn_create(parent);
    lv_obj_set_pos(bgoto, LX, y);
    lv_obj_set_size(bgoto, 130, 56);
    lv_obj_set_style_bg_color(bgoto, lv_color_make(0x1E, 0x45, 0x80), 0);
    lv_obj_set_style_bg_color(bgoto, lv_color_make(0x18, 0x38, 0x68), LV_STATE_PRESSED);
    lv_obj_set_style_radius(bgoto, 10, 0);
    lv_obj_set_style_shadow_width(bgoto, 0, 0);
    lv_obj_add_event_cb(bgoto, goto_cb, LV_EVENT_CLICKED, nullptr);
    { lv_obj_t *l = lv_label_create(bgoto);
            lv_label_set_text(l, g_language == LANG_SERBIAN ? LV_SYMBOL_EDIT "\nIDI NA" : LV_SYMBOL_EDIT "\nGO TO");
      lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(l, lv_color_white(), 0);
      lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_center(l); }

    s_go_btn = lv_btn_create(parent);
    lv_obj_set_pos(s_go_btn, LX + 130 + 6, y);
    lv_obj_set_size(s_go_btn, 240, 56);
    lv_obj_set_style_bg_color(s_go_btn, lv_color_make(0x22, 0x2C, 0x44), 0);
    lv_obj_set_style_bg_color(s_go_btn, lv_color_make(0x1A, 0x6E, 0x1A), LV_STATE_PRESSED);
    lv_obj_set_style_radius(s_go_btn, 10, 0);
    lv_obj_set_style_shadow_width(s_go_btn, 0, 0);
    lv_obj_clear_flag(s_go_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_go_btn, execute_cb, LV_EVENT_CLICKED, nullptr);
    { lv_obj_t *l = lv_label_create(s_go_btn);
            lv_label_set_text(l, g_language == LANG_SERBIAN ? LV_SYMBOL_PLAY "  POKRENI" : LV_SYMBOL_PLAY "  START");
      lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_color(l, lv_color_white(), 0);
      lv_obj_center(l); }

    lv_obj_t *bstop = lv_btn_create(parent);
    lv_obj_set_pos(bstop, LX + 130 + 6 + 240 + 6, y);
    lv_obj_set_size(bstop, LW - 130 - 6 - 240 - 6, 56);
    lv_obj_set_style_bg_color(bstop, CLR_BTN_STOP, 0);
    lv_obj_set_style_bg_color(bstop, lv_color_make(0xB7, 0x10, 0x10), LV_STATE_PRESSED);
    lv_obj_set_style_radius(bstop, 10, 0);
    lv_obj_set_style_shadow_width(bstop, 0, 0);
    lv_obj_add_event_cb(bstop, stop_cb, LV_EVENT_CLICKED, nullptr);
    { lv_obj_t *l = lv_label_create(bstop);
      lv_label_set_text(l, LV_SYMBOL_STOP "\nSTOP");
      lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(l, lv_color_white(), 0);
      lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_center(l); }

    // 4) HOMING + RETRACT switch — spušteno pri dnu
    lv_obj_t *bhome = lv_btn_create(parent);
    lv_obj_set_pos(bhome, LX, HOME_Y);
    lv_obj_set_size(bhome, 230, 56);
    lv_obj_set_style_bg_color(bhome, lv_color_make(0x4A, 0x14, 0x8C), 0);
    lv_obj_set_style_bg_color(bhome, lv_color_make(0x38, 0x0E, 0x6E), LV_STATE_PRESSED);
    lv_obj_set_style_radius(bhome, 10, 0);
    lv_obj_set_style_shadow_width(bhome, 0, 0);
    lv_obj_add_event_cb(bhome, home_cb, LV_EVENT_CLICKED, nullptr);
    { lv_obj_t *l = lv_label_create(bhome);
      lv_label_set_text(l, LV_SYMBOL_HOME "  HOMING");
      lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_color(l, lv_color_white(), 0);
      lv_obj_center(l); }

    lv_obj_t *retcard = lv_obj_create(parent);
    lv_obj_set_pos(retcard, LX + 230 + 8, HOME_Y);
    lv_obj_set_size(retcard, LW - 230 - 8, 56);
    lv_obj_set_style_bg_color(retcard, lv_color_make(0x10, 0x18, 0x2C), 0);
    lv_obj_set_style_border_color(retcard, CLR_BORDER, 0);
    lv_obj_set_style_border_width(retcard, 1, 0);
    lv_obj_set_style_radius(retcard, 10, 0);
    lv_obj_set_style_pad_all(retcard, 0, 0);
    lv_obj_clear_flag(retcard, LV_OBJ_FLAG_SCROLLABLE);
    { lv_obj_t *l = lv_label_create(retcard);
      lv_label_set_text(l, LV_SYMBOL_REFRESH "  AUTO RETRACT");
      lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(l, CLR_TEXT_DIM, 0);
      lv_obj_align(l, LV_ALIGN_LEFT_MID, 10, 0); }
    lv_obj_t *rsw = lv_switch_create(retcard);
    lv_obj_align(rsw, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(rsw, lv_color_make(0x2E, 0x7D, 0x32),
                               LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (g_retract_enabled) lv_obj_add_state(rsw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(rsw, [](lv_event_t *e) {
        lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
        g_retract_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
        espnow_hmi_send_sync();
    }, LV_EVENT_VALUE_CHANGED, nullptr);





    s_statusbar = ui_statusbar_create(parent);

    // Timer 50ms: osvjezava prikaz iz g_machine.position_mm (~20Hz, prati motor)
    s_pos_timer = lv_timer_create(pos_sync_cb, 50, nullptr);

    lv_obj_add_event_cb(parent, [](lv_event_t *) {
        if (s_pos_timer) { lv_timer_del(s_pos_timer); s_pos_timer = nullptr; }
        s_cur_label = s_tgt_label = s_mac_label = s_go_btn = s_statusbar = nullptr;
        for (int i = 0; i < INC_COUNT; i++) s_inc_btns[i] = nullptr;
    }, LV_EVENT_DELETE, nullptr);

    update_display();
    refresh_increment_labels();
    select_increment(s_inc_idx);
}