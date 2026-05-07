#include "ui_settings.h"
#include "ui_common.h"
#include "storage.h"
#include "data_model.h"
#include "espnow_hmi.h"
#include "ui_nav.h"
#include "ui_strings.h"
#include <lvgl.h>
#include <Arduino.h>

// ====================================================================
//  Settings screen — fully static layout, no lv_obj_align inside
//  row children, no lambdas-in-loops, no Serial dependency.
//
//  Layout (800x480, topbar=50, statusbar=36):
//    content y = 50..444  (394px usable)
//    y=56  ..  y=396 : 7 rows × (46+4) = 350px
//    y=406 : save button h=32
//    y=444 : statusbar
// ====================================================================

struct Field {
    const char *label;
    float      *value;
    float       vmin, vmax;
    const char *unit;
};

// Sekcije: MEHANIKA(0-1) | BRZINE(2-4) | HOMING(5-9) | RETRACT(10-11,14) | POZICIONIRANJE(12-13)
static Field FIELDS[] = {
    /* 0 */ { "KORACI / MM",    &g_settings.steps_per_mm,         1,  10000, "stp/mm" },
    /* 1 */ { "MAX HOD",        &g_settings.max_travel_mm,        10,   2000, "mm"     },
    /* 2 */ { "JOG BRZINA",     &g_settings.jog_speed_mmps,        1,    500, "mm/s"   },
    /* 3 */ { "AUTO BRZINA",    &g_settings.auto_speed_mmps,       1,    500, "mm/s"   },
    /* 4 */ { "AKCELERACIJA",   &g_settings.accel_mmps2,           1,   3000, "mm/s2"  },
    /* 5 */ { "HOME OFFSET",    &g_settings.home_offset_mm,      -50,   2000, "mm"     },
    /* 6 */ { "HOMING BRZA",    &g_settings.home_fast_speed_mmps,  1,    400, "mm/s"   },
    /* 7 */ { "HOMING ODMAK",   &g_settings.home_odmak_mm,         1,     30, "mm"     },
    /* 8 */ { "ODMAK OD SENZ.", &g_settings.home_clearance_mm,    1,     50, "mm"     },
    /* 9 */ { "HOMING SPORA",   &g_settings.home_speed_mmps,       1,    100, "mm/s"   },
    /*10 */ { "RETRACT",        &g_settings.retract_mm,            0,     50, "mm"     },
    /*11 */ { "RETRACT BRZINA", &g_settings.retract_speed_mmps,    1,    300, "mm/s"   },
    /*12 */ { "PREKORACENJE",   &g_settings.overshoot_mm,          0,     20, "mm"     },
    /*13 */ { "BRZINA PRIL.",   &g_settings.approach_speed_mmps,   1,    100, "mm/s"   },
    /*14 */ { "PAUZA RETRAKTA",&g_settings.auto_retract_pause_s,  0,     30, "s"      },
};
static const int NFIELDS = 15;

static lv_obj_t *s_val_lbls[15] = {};
static lv_obj_t *s_dir_lbl           = nullptr;
static lv_obj_t *s_sb                = nullptr;
static lv_obj_t *s_lang_lbl          = nullptr;

static void apply_field_labels()
{
    if (g_language == LANG_ENGLISH) {
        FIELDS[0].label = "STEPS / MM";
        FIELDS[1].label = "MAX TRAVEL";
        FIELDS[2].label = "JOG SPEED";
        FIELDS[3].label = "AUTO SPEED";
        FIELDS[4].label = "ACCELERATION";
        FIELDS[5].label = "HOME OFFSET";
        FIELDS[6].label = "HOMING FAST";
        FIELDS[7].label = "HOMING BACKOFF";
        FIELDS[8].label = "SENSOR CLEAR.";
        FIELDS[9].label = "HOMING SLOW";
        FIELDS[10].label = "RETRACT";
        FIELDS[11].label = "RETRACT SPEED";
        FIELDS[12].label = "OVERSHOOT";
        FIELDS[13].label = "APPROACH SPEED";
        FIELDS[14].label = "RETRACT PAUSE";
        return;
    }

    FIELDS[0].label = "KORACI / MM";
    FIELDS[1].label = "MAX HOD";
    FIELDS[2].label = "JOG BRZINA";
    FIELDS[3].label = "AUTO BRZINA";
    FIELDS[4].label = "AKCELERACIJA";
    FIELDS[5].label = "HOME OFFSET";
    FIELDS[6].label = "HOMING BRZA";
    FIELDS[7].label = "HOMING ODMAK";
    FIELDS[8].label = "ODMAK OD SENZ.";
    FIELDS[9].label = "HOMING SPORA";
    FIELDS[10].label = "RETRACT";
    FIELDS[11].label = "RETRACT BRZINA";
    FIELDS[12].label = "PREKORACENJE";
    FIELDS[13].label = "BRZINA PRIL.";
    FIELDS[14].label = "PAUZA RETRAKTA";
}

// --------------------------------------------------------------------
static void refresh_vals()
{
    char buf[32];
    for (int i = 0; i < NFIELDS; i++) {
        if (!s_val_lbls[i]) continue;
        snprintf(buf, sizeof(buf), "%.2f %s",
                    *FIELDS[i].value, FIELDS[i].unit);
        lv_label_set_text(s_val_lbls[i], buf);
    }
}

// Forward
static void numpad_done(float val, void *ud);

static void edit_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= NFIELDS) return;
    ui_numpad_open(FIELDS[i].label, *FIELDS[i].value,
                   numpad_done, (void *)(intptr_t)i);
}

static void numpad_done(float val, void *ud)
{
    int i = (int)(intptr_t)ud;
    if (i < 0 || i >= NFIELDS) return;
    if (val < FIELDS[i].vmin) val = FIELDS[i].vmin;
    if (val > FIELDS[i].vmax) val = FIELDS[i].vmax;
    *FIELDS[i].value = val;
    refresh_vals();
    if (s_sb)
        ui_statusbar_set(s_sb,
            g_language == LANG_SERBIAN
                ? LV_SYMBOL_EDIT "  IZMENJENO - ne zaboravi SNIMI"
                : LV_SYMBOL_EDIT "  CHANGED - don't forget SAVE",
            CLR_YELLOW);
}

static void save_cb(lv_event_t *)
{
    storage_save_settings();
    espnow_hmi_send_sync();   // odmah sinhroniziraj motor
    if (s_sb)
        ui_statusbar_set(s_sb,
            g_language == LANG_SERBIAN
                ? LV_SYMBOL_OK "  PODESAVANJA SNIMLJENA"
                : LV_SYMBOL_OK "  SETTINGS SAVED",
            CLR_GREEN);
}

static void dir_cb(lv_event_t *)
{
    g_settings.direction = (g_settings.direction == 1) ? -1 : 1;
    if (s_dir_lbl)
        lv_label_set_text(s_dir_lbl,
            g_settings.direction == 1
                ? (g_language == LANG_SERBIAN ? "NORMALNO" : "NORMAL")
                : (g_language == LANG_SERBIAN ? "INVERTIRANO" : "INVERTED"));
    if (s_sb)
        ui_statusbar_set(s_sb,
            g_language == LANG_SERBIAN
                ? LV_SYMBOL_EDIT "  SMER PROMENJEN - ne zaboravi SNIMI"
                : LV_SYMBOL_EDIT "  DIRECTION CHANGED - don't forget SAVE",
            CLR_YELLOW);
}

static void lang_cb(lv_event_t *)
{
    set_language(g_language == LANG_SERBIAN ? LANG_ENGLISH : LANG_SERBIAN);
    ui_nav_go(SCREEN_SETTINGS);
}

// --------------------------------------------------------------------
//  Row builder
//  Row: x=8, w=784, h=44
//    x=14  : labela  font_16 CLR_TEXT_DIM
//    x=340 : vrijednost  font_16 CLR_ACCENT
//    x=686 : dugme w=90 h=32
// --------------------------------------------------------------------
static lv_obj_t *make_row(lv_obj_t *parent, int y,
                           const char *lbl_txt,
                           const char *val_txt,
                           lv_event_cb_t cb,
                           void *ud,
                           const char *btn_icon)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_pos(row, 8, y);
    lv_obj_set_size(row, 784, 44);
    lv_obj_set_style_bg_color(row, lv_color_make(0x10, 0x18, 0x2C), 0);
    lv_obj_set_style_border_color(row, CLR_BORDER, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_radius(row, 8, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ll = lv_label_create(row);
    lv_label_set_text(ll, lbl_txt);
    lv_obj_set_style_text_font(ll, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ll, CLR_TEXT_DIM, 0);
    lv_obj_set_pos(ll, 14, 13);

    lv_obj_t *vl = lv_label_create(row);
    lv_label_set_text(vl, val_txt);
    lv_obj_set_style_text_font(vl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(vl, CLR_ACCENT, 0);
    lv_obj_set_pos(vl, 340, 13);

    lv_obj_t *btn = lv_btn_create(row);
    lv_obj_set_pos(btn, 686, 6);
    lv_obj_set_size(btn, 90, 32);
    lv_obj_set_style_bg_color(btn, lv_color_make(0x28, 0x3A, 0x5C), 0);
    lv_obj_set_style_bg_color(btn,
        lv_color_make(0x18, 0x2A, 0x46), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, ud);

    lv_obj_t *bi = lv_label_create(btn);
    lv_label_set_text(bi, btn_icon);
    lv_obj_set_style_text_font(bi, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(bi, CLR_TEXT, 0);
    lv_obj_align(bi, LV_ALIGN_CENTER, 0, 0);

    return vl;
}

// Pomocna: sekcijski naslov u scroll containeru
static void make_section(lv_obj_t *parent, int y, const char *title)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_pos(row, 8, y);
    lv_obj_set_size(row, 784, 26);
    lv_obj_set_style_bg_color(row, lv_color_make(0x08, 0x0D, 0x18), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, CLR_YELLOW, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);
}

// --------------------------------------------------------------------
void ui_settings_create(lv_obj_t *parent)
{
    apply_field_labels();

    s_sb      = nullptr;
    s_dir_lbl = nullptr;
    for (int i = 0; i < NFIELDS; i++) s_val_lbls[i] = nullptr;

    const int ROW_H    = 44;
    const int GAP      = 4;
    const int SEC_H    = 26;
    const int SGAP     = 8;   // razmak prije sekcijskog naslova
    const int FOOTER_H = 54;
    const int SCROLL_H = CONTENT_H - FOOTER_H;

    // ── Scrollable kontejner ───────────────────────────────────────────
    lv_obj_t *sc = lv_obj_create(parent);
    lv_obj_set_pos(sc, 0, CONTENT_Y);
    lv_obj_set_size(sc, 800, SCROLL_H);
    lv_obj_set_style_bg_color(sc, CLR_BG, 0);
    lv_obj_set_style_bg_opa(sc, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sc, 0, 0);
    lv_obj_set_style_pad_all(sc, 0, 0);
    lv_obj_set_scroll_dir(sc, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(sc, LV_SCROLLBAR_MODE_ACTIVE);

    // helper lambda za redove
    auto add_row = [&](lv_obj_t *p, int &y, int idx) {
        char vbuf[32];
        snprintf(vbuf, sizeof(vbuf), "%.2f %s", *FIELDS[idx].value, FIELDS[idx].unit);
        s_val_lbls[idx] = make_row(p, y, FIELDS[idx].label, vbuf,
                                    edit_cb, (void *)(intptr_t)idx, LV_SYMBOL_EDIT);
        y += ROW_H + GAP;
    };

    int y = 6;

    // ─── JEZIK ─────────────────────────────────────────────────────────
    make_section(sc, y,
        g_language == LANG_SERBIAN
            ? LV_SYMBOL_SETTINGS "  JEZIK"
            : LV_SYMBOL_SETTINGS "  LANGUAGE");
    y += SEC_H + 2;
    char lang_buf[20];
    snprintf(lang_buf, sizeof(lang_buf), "%s",
             g_language == LANG_SERBIAN ? "Srpski" : "English");
    s_lang_lbl = make_row(sc, y,
                           g_language == LANG_SERBIAN ? "Jezik" : "Language",
                           lang_buf,
                           lang_cb, nullptr, LV_SYMBOL_LOOP);
    y += ROW_H + GAP;

    // ─── MEHANIKA ─────────────────────────────────────────────────────
    make_section(sc, y,
        g_language == LANG_SERBIAN
            ? LV_SYMBOL_SETTINGS "  MEHANIKA"
            : LV_SYMBOL_SETTINGS "  MECHANICS");
    y += SEC_H + 2;
    add_row(sc, y, 0);  // KORACI/MM
    add_row(sc, y, 1);  // MAX HOD

    // ─── BRZINE ─────────────────────────────────────────────────────────
    y += SGAP;
    make_section(sc, y,
        g_language == LANG_SERBIAN
            ? LV_SYMBOL_PLAY "  BRZINE"
            : LV_SYMBOL_PLAY "  SPEEDS");
    y += SEC_H + 2;
    add_row(sc, y, 2);  // JOG BRZINA
    add_row(sc, y, 3);  // AUTO BRZINA
    add_row(sc, y, 4);  // AKCELERACIJA

    // ─── HOMING ─────────────────────────────────────────────────────────
    y += SGAP;
    make_section(sc, y, LV_SYMBOL_HOME "  HOMING"); y += SEC_H + 2;
    add_row(sc, y, 5);  // HOME OFFSET
    add_row(sc, y, 6);  // HOMING BRZA
    add_row(sc, y, 7);  // HOMING ODMAK
    add_row(sc, y, 8);  // ODMAK OD SENZ.
    add_row(sc, y, 9);  // HOMING SPORA
    // SMER OKRETANJA (toggle)
    char dir_buf[20];
    snprintf(dir_buf, sizeof(dir_buf), "%s",
                 g_settings.direction == 1
                     ? (g_language == LANG_SERBIAN ? "NORMALNO" : "NORMAL")
                     : (g_language == LANG_SERBIAN ? "INVERTIRANO" : "INVERTED"));
     s_dir_lbl = make_row(sc, y,
                                 g_language == LANG_SERBIAN ? "SMER OKRETANJA" : "ROTATION DIRECTION",
                                 dir_buf,
                         dir_cb, nullptr, LV_SYMBOL_LOOP);
    y += ROW_H + GAP;

    // ─── RETRACT ─────────────────────────────────────────────────────────
    y += SGAP;
    make_section(sc, y, LV_SYMBOL_REFRESH "  RETRACT"); y += SEC_H + 2;
    add_row(sc, y, 10); // RETRACT MM
    add_row(sc, y, 11); // RETRACT BRZINA
    add_row(sc, y, 14); // PAUZA RETRAKTA

    // ─── POZICIONIRANJE ────────────────────────────────────────────────
    y += SGAP;
    make_section(sc, y,
        g_language == LANG_SERBIAN
            ? LV_SYMBOL_GPS "  POZICIONIRANJE"
            : LV_SYMBOL_GPS "  POSITIONING");
    y += SEC_H + 2;
    add_row(sc, y, 12); // PREKORACENJE
    add_row(sc, y, 13); // BRZINA PRIL.

    // ── SNIMI footer — uvijek vidljiv ──────────────────────────────────
    lv_obj_t *footer = lv_obj_create(parent);
    lv_obj_set_pos(footer, 0, CONTENT_Y + SCROLL_H);
    lv_obj_set_size(footer, 800, FOOTER_H);
    lv_obj_set_style_bg_color(footer, lv_color_make(0x0D, 0x11, 0x20), 0);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(footer, 1, 0);
    lv_obj_set_style_border_side(footer, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(footer, CLR_BORDER, 0);
    lv_obj_set_style_pad_all(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *sav = lv_btn_create(footer);
    lv_obj_set_size(sav, 360, 40);
    lv_obj_align(sav, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(sav, CLR_BTN_GO, 0);
    lv_obj_set_style_bg_color(sav, lv_color_make(0x18, 0x5E, 0x18), LV_STATE_PRESSED);
    lv_obj_set_style_radius(sav, 8, 0);
    lv_obj_set_style_shadow_width(sav, 0, 0);
    lv_obj_add_event_cb(sav, save_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *sl = lv_label_create(sav);
    lv_label_set_text(sl,
        g_language == LANG_SERBIAN
            ? LV_SYMBOL_OK "  SNIMI PODESAVANJA"
            : LV_SYMBOL_OK "  SAVE SETTINGS");
    lv_obj_set_style_text_font(sl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(sl, lv_color_white(), 0);
    lv_obj_align(sl, LV_ALIGN_CENTER, 0, 0);

    s_sb = ui_statusbar_create(parent);

    lv_obj_add_event_cb(parent, [](lv_event_t *) {
        s_sb = s_dir_lbl = s_lang_lbl = nullptr;
        for (int i = 0; i < NFIELDS; i++) s_val_lbls[i] = nullptr;
    }, LV_EVENT_DELETE, nullptr);
}

