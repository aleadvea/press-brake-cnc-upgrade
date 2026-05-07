#include "ui_common.h"
#include "ui_nav.h"
#include "ui_strings.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <Arduino.h>

// ------------------------------------------------------------------ //
//  Interni statics                                                    //
// ------------------------------------------------------------------ //
static lv_obj_t  *s_clock_label = nullptr;

// ------------------------------------------------------------------ //
//  Topbar                                                             //
// ------------------------------------------------------------------ //
void ui_topbar_create(lv_obj_t *parent, const char *title, bool show_home_btn)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, 800, TOPBAR_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, CLR_TOPBAR, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(bar, 2, 0);
    lv_obj_set_style_border_color(bar, CLR_BORDER, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    int title_x = 14;

    if (show_home_btn) {
        // HOME dugme
        lv_obj_t *btn = lv_btn_create(bar);
        lv_obj_set_size(btn, 40, 34);
        lv_obj_align(btn, LV_ALIGN_LEFT_MID, 8, 0);
        lv_obj_set_style_bg_color(btn, CLR_BORDER, 0);
        lv_obj_set_style_bg_color(btn, lv_color_make(0x42, 0x55, 0x80), LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_pad_all(btn, 4, 0);
        lv_obj_add_event_cb(btn, [](lv_event_t *) { ui_nav_home(); },
                            LV_EVENT_CLICKED, nullptr);
        lv_obj_t *ico = lv_label_create(btn);
        lv_label_set_text(ico, LV_SYMBOL_HOME);
        lv_obj_set_style_text_color(ico, CLR_ACCENT, 0);
        lv_obj_center(ico);
        title_x = 58;
    }

    // Naslov
    lv_obj_t *lbl = lv_label_create(bar);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, CLR_TEXT, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, title_x, 0);

    // Sat
    s_clock_label = lv_label_create(bar);
    lv_label_set_text(s_clock_label, "00:00:00");
    lv_obj_set_style_text_font(s_clock_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_clock_label, CLR_ACCENT, 0);
    lv_obj_align(s_clock_label, LV_ALIGN_RIGHT_MID, -14, 0);

    // Timer za sat — user_data = clock label za OVAJ topbar specificno
    lv_timer_t *clk_tmr = lv_timer_create([](lv_timer_t *t) {
        lv_obj_t *lbl = (lv_obj_t *)t->user_data;
        if (!lbl) return;
        uint32_t secs = millis() / 1000;
        char buf[12];
        snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                    (unsigned)(secs / 3600),
                    (unsigned)((secs % 3600) / 60),
                    (unsigned)(secs % 60));
        lv_label_set_text(lbl, buf);
    }, 1000, s_clock_label);
    // Kad bar bude obrisan: zaustavi SOPSTVENI timer, ponisti user_data
    lv_obj_add_event_cb(bar, [](lv_event_t *e) {
        lv_timer_t *t = (lv_timer_t *)lv_event_get_user_data(e);
        if (!t) return;
        lv_obj_t *lbl = (lv_obj_t *)t->user_data;
        t->user_data = nullptr;          // sprecava crash ako timer stigne prije brisanja
        lv_timer_del(t);
        if (s_clock_label == lbl) s_clock_label = nullptr; // null samo ako je nas label
    }, LV_EVENT_DELETE, clk_tmr);
    ui_topbar_tick();
}

void ui_topbar_tick()
{
    if (!s_clock_label) return;
    uint32_t secs = millis() / 1000;
    char buf[12];
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                (unsigned)(secs / 3600),
                (unsigned)((secs % 3600) / 60),
                (unsigned)(secs % 60));
    lv_label_set_text(s_clock_label, buf);
}

// ------------------------------------------------------------------ //
//  Status bar                                                         //
// ------------------------------------------------------------------ //
lv_obj_t *ui_statusbar_create(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, 800, STATUSBAR_H);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, CLR_TOPBAR, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(bar, 2, 0);
    lv_obj_set_style_border_color(bar, CLR_BORDER, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 4, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(bar);
    lv_label_set_text(lbl,
        g_language == LANG_SERBIAN
            ? LV_SYMBOL_OK "  SPREMAN"
            : LV_SYMBOL_OK "  READY");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, CLR_GREEN, 0);
    lv_obj_center(lbl);
    return bar;
}

void ui_statusbar_set(lv_obj_t *bar, const char *msg, lv_color_t color)
{
    if (!bar) return;
    lv_obj_t *lbl = lv_obj_get_child(bar, 0);
    if (lbl) {
        lv_label_set_text(lbl, msg);
        lv_obj_set_style_text_color(lbl, color, 0);
    }
}

// ------------------------------------------------------------------ //
//  Card                                                               //
// ------------------------------------------------------------------ //
lv_obj_t *ui_card_create(lv_obj_t *parent, int x, int y, int w, int h,
                          const char *title)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, CLR_CARD, 0);
    lv_obj_set_style_border_color(card, CLR_BORDER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    if (title && title[0]) {
        lv_obj_t *hdr = lv_obj_create(card);
        lv_obj_set_size(hdr, w, 28);
        lv_obj_set_pos(hdr, 0, 0);
        lv_obj_set_style_bg_color(hdr, CLR_BORDER, 0);
        lv_obj_set_style_radius(hdr, 0, 0);
        lv_obj_set_style_border_width(hdr, 0, 0);
        lv_obj_set_style_pad_all(hdr, 0, 0);
        lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl = lv_label_create(hdr);
        lv_label_set_text(lbl, title);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, CLR_TEXT_DIM, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);
    }
    return card;
}

lv_obj_t *ui_card_body(lv_obj_t *card)
{
    // Body je card sam po sebi (layout se dodaje izvana)
    return card;
}

// ------------------------------------------------------------------ //
//  Dugme                                                              //
// ------------------------------------------------------------------ //
lv_obj_t *ui_btn_create(lv_obj_t *parent, const char *label,
                          lv_color_t color, int w, int h)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_set_style_bg_grad_color(btn, lv_color_darken(color, LV_OPA_20), 0);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_darken(color, LV_OPA_30), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_shadow_width(btn, 8, 0);
    lv_obj_set_style_shadow_color(btn, lv_color_darken(color, LV_OPA_50), 0);
    lv_obj_set_style_shadow_ofs_y(btn, 3, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_40, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);
    return btn;
}

// ------------------------------------------------------------------ //
//  Numericka tastatura (modal)                                        //
// ------------------------------------------------------------------ //
struct NumpadCtx {
    numpad_done_cb  cb;
    void           *user_data;
    lv_obj_t       *input;
    lv_obj_t       *overlay;
    char            buf[16];
};

static NumpadCtx s_numpad;

static void numpad_key_cb(lv_event_t *e)
{
    const char *key = (const char *)lv_event_get_user_data(e);
    size_t len = strlen(s_numpad.buf);

    if (strcmp(key, "DEL") == 0) {
        if (len > 0) s_numpad.buf[len - 1] = '\0';
    } else if (strcmp(key, ".") == 0) {
        // samo jedan decimal
        if (strchr(s_numpad.buf, '.') == nullptr && len < 9)
            strcat(s_numpad.buf, ".");
    } else if (strcmp(key, "OK") == 0) {
        float val = atof(s_numpad.buf);
        lv_obj_del(s_numpad.overlay);
        s_numpad.overlay = nullptr;
        if (s_numpad.cb) s_numpad.cb(val, s_numpad.user_data);
        return;
    } else if (strcmp(key, "X") == 0) {
        lv_obj_del(s_numpad.overlay);
        s_numpad.overlay = nullptr;
        return;
    } else {
        if (len < 9) strcat(s_numpad.buf, key);
    }
    lv_label_set_text(s_numpad.input, s_numpad.buf);
}

static lv_obj_t *numpad_add_key(lv_obj_t *grid, const char *label,
                                  int col, int row, int colspan = 1)
{
    lv_obj_t *btn = lv_btn_create(grid);
    lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, colspan,
                              LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_style_bg_color(btn, CLR_BORDER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_make(0x40, 0x55, 0x80), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, CLR_TEXT, 0);
    lv_obj_center(lbl);
    // kopiraj label kao user_data (staticni string)
    lv_obj_add_event_cb(btn, numpad_key_cb, LV_EVENT_CLICKED, (void *)label);
    return btn;
}

void ui_numpad_open(const char *title, float current_val,
                     numpad_done_cb cb, void *user_data)
{
    s_numpad.cb        = cb;
    s_numpad.user_data = user_data;
    if (std::isnan(current_val)) {
        s_numpad.buf[0] = '\0';
    } else {
        snprintf(s_numpad.buf, sizeof(s_numpad.buf), "%.3f", current_val);
        // Ukloni trailing nule
        char *dot = strchr(s_numpad.buf, '.');
        if (dot) {
            char *end = s_numpad.buf + strlen(s_numpad.buf) - 1;
            while (end > dot && *end == '0') *end-- = '\0';
            if (*end == '.') *end = '\0';
        }
    }

    // Overlay
    lv_obj_t *ov = lv_obj_create(lv_scr_act());
    s_numpad.overlay = ov;
    lv_obj_set_size(ov, 800, 480);
    lv_obj_set_pos(ov, 0, 0);
    lv_obj_set_style_bg_color(ov, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(ov, LV_OPA_70, 0);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);

    // Dialog box
    lv_obj_t *box = lv_obj_create(ov);
    lv_obj_set_size(box, 340, 380);
    lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(box, CLR_CARD, 0);
    lv_obj_set_style_border_color(box, CLR_BORDER, 0);
    lv_obj_set_style_radius(box, 14, 0);
    lv_obj_set_style_pad_all(box, 12, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    // Naslov dialoga
    lv_obj_t *ttl = lv_label_create(box);
    lv_label_set_text(ttl, title);
    lv_obj_set_style_text_font(ttl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ttl, CLR_TEXT_DIM, 0);
    lv_obj_align(ttl, LV_ALIGN_TOP_MID, 0, 0);

    // Display vrednosti
    lv_obj_t *disp = lv_obj_create(box);
    lv_obj_set_size(disp, 310, 44);
    lv_obj_align(disp, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_set_style_bg_color(disp, lv_color_make(0x08, 0x0A, 0x14), 0);
    lv_obj_set_style_border_color(disp, CLR_ACCENT, 0);
    lv_obj_set_style_radius(disp, 8, 0);
    lv_obj_set_style_pad_ver(disp, 4, 0);
    lv_obj_set_style_pad_hor(disp, 10, 0);
    lv_obj_clear_flag(disp, LV_OBJ_FLAG_SCROLLABLE);

    s_numpad.input = lv_label_create(disp);
    lv_label_set_text(s_numpad.input, s_numpad.buf);
    lv_obj_set_style_text_font(s_numpad.input, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_numpad.input, CLR_ACCENT, 0);
    lv_obj_align(s_numpad.input, LV_ALIGN_RIGHT_MID, 0, 0);

    // Grid za tastere: 3 kolone x 5 redova
    lv_obj_t *grid = lv_obj_create(box);
    lv_obj_set_size(grid, 310, 280);
    lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_style_pad_row(grid, 6, 0);
    lv_obj_set_style_pad_column(grid, 6, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    static lv_coord_t cols[] = { LV_PCT(33), LV_PCT(33), LV_PCT(34), LV_GRID_TEMPLATE_LAST };
    static lv_coord_t rows[] = { LV_PCT(20), LV_PCT(20), LV_PCT(20), LV_PCT(20), LV_PCT(20), LV_GRID_TEMPLATE_LAST };
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, cols, rows);

    static const char *keys[15] = {
        "7","8","9",
        "4","5","6",
        "1","2","3",
        ".","0","DEL",
        "X","","OK"
    };
    // red 0-3: cifre + dec
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 3; c++) {
            const char *k = keys[r * 3 + c];
            if (k && k[0]) {
                lv_obj_t *b = numpad_add_key(grid, k, c, r);
                if (strcmp(k, "DEL") == 0)
                    lv_obj_set_style_bg_color(b, lv_color_make(0x6D, 0x30, 0x30), 0);
            }
        }
    }
    // red 4: X, (prazan), OK
    lv_obj_t *bx = numpad_add_key(grid, "X", 0, 4);
    lv_obj_set_style_bg_color(bx, CLR_BTN_STOP, 0);
    numpad_add_key(grid, "OK", 2, 4);
    lv_obj_t *bok = lv_obj_get_child(grid, lv_obj_get_child_cnt(grid) - 1);
    lv_obj_set_style_bg_color(bok, CLR_BTN_GO, 0);
    (void)bok;
}

void ui_numpad_open_empty(const char *title,
                          numpad_done_cb cb, void *user_data)
{
    ui_numpad_open(title, NAN, cb, user_data);
}

// ------------------------------------------------------------------ //
//  Modal potvrda                                                      //
// ------------------------------------------------------------------ //
struct ConfirmCtx { confirm_cb cb; void *ud; lv_obj_t *ov; };
static ConfirmCtx s_confirm;

static void confirm_btn_cb(lv_event_t *e)
{
    bool yes = (bool)(intptr_t)lv_event_get_user_data(e);
    lv_obj_del(s_confirm.ov);
    s_confirm.ov = nullptr;
    if (s_confirm.cb) s_confirm.cb(yes, s_confirm.ud);
}

void ui_confirm_open(const char *msg, confirm_cb cb, void *user_data)
{
    s_confirm.cb = cb;
    s_confirm.ud = user_data;

    lv_obj_t *ov = lv_obj_create(lv_scr_act());
    s_confirm.ov = ov;
    lv_obj_set_size(ov, 800, 480);
    lv_obj_set_pos(ov, 0, 0);
    lv_obj_set_style_bg_color(ov, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(ov, LV_OPA_70, 0);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *box = lv_obj_create(ov);
    lv_obj_set_size(box, 400, 160);
    lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(box, CLR_CARD, 0);
    lv_obj_set_style_border_color(box, CLR_BORDER, 0);
    lv_obj_set_style_radius(box, 14, 0);
    lv_obj_set_style_pad_all(box, 16, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(box);
    lv_label_set_text(lbl, msg);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, CLR_TEXT, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *bno = ui_btn_create(box, g_language == LANG_SERBIAN ? "NE" : "NO", CLR_BTN_STOP, 140, 44);
    lv_obj_align(bno, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_event_cb(bno, confirm_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)false);

    lv_obj_t *byes = ui_btn_create(box, g_language == LANG_SERBIAN ? "DA" : "YES", CLR_BTN_GO, 140, 44);
    lv_obj_align(byes, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(byes, confirm_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)true);
}

// ------------------------------------------------------------------ //
//  Tastatura (modal za unos teksta)                                   //
// ------------------------------------------------------------------ //
struct KbCtx { keyboard_done_cb cb; void *ud; lv_obj_t *ov; lv_obj_t *ta; };
static KbCtx  s_kb;
static char   s_kb_text[32];

static void kb_ok()
{
    if (!s_kb.ov || !s_kb.ta) return;
    strncpy(s_kb_text, lv_textarea_get_text(s_kb.ta), 31);
    s_kb_text[31] = '\0';
    keyboard_done_cb cb = s_kb.cb;
    void *ud = s_kb.ud;
    lv_obj_del(s_kb.ov);
    s_kb.ov = s_kb.ta = nullptr;
    if (cb) cb(s_kb_text, ud);
}

static void kb_cancel()
{
    if (!s_kb.ov) return;
    lv_obj_del(s_kb.ov);
    s_kb.ov = s_kb.ta = nullptr;
}

void ui_keyboard_open(const char *title, const char *current,
                      keyboard_done_cb cb, void *ud)
{
    s_kb.cb = cb;
    s_kb.ud = ud;

    lv_obj_t *ov = lv_obj_create(lv_scr_act());
    s_kb.ov = ov;
    lv_obj_set_size(ov, 800, 480);
    lv_obj_set_pos(ov, 0, 0);
    lv_obj_set_style_bg_color(ov, lv_color_make(0x05, 0x07, 0x10), 0);
    lv_obj_set_style_bg_opa(ov, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ttl = lv_label_create(ov);
    lv_label_set_text(ttl, title);
    lv_obj_set_style_text_font(ttl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ttl, CLR_TEXT, 0);
    lv_obj_align(ttl, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *ta = lv_textarea_create(ov);
    s_kb.ta = ta;
    lv_textarea_set_text(ta, current ? current : "");
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 31);
    lv_obj_set_size(ta, 680, 50);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 36);
    lv_obj_set_style_bg_color(ta, CLR_CARD, 0);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ta, CLR_TEXT, 0);
    lv_obj_set_style_border_color(ta, CLR_ACCENT, 0);
    lv_obj_set_style_border_width(ta, 2, 0);

    lv_obj_t *bok = ui_btn_create(ov, LV_SYMBOL_OK " OK",
                                   CLR_BTN_GO, 160, 42);
    lv_obj_align(bok, LV_ALIGN_TOP_MID, 90, 98);
    lv_obj_add_event_cb(bok, [](lv_event_t *) { kb_ok(); },
                        LV_EVENT_CLICKED, nullptr);

    lv_obj_t *bx = ui_btn_create(ov,
                                  g_language == LANG_SERBIAN ? LV_SYMBOL_CLOSE " OTKAZ" : LV_SYMBOL_CLOSE " CANCEL",
                                  CLR_BTN_STOP, 160, 42);
    lv_obj_align(bx, LV_ALIGN_TOP_MID, -90, 98);
    lv_obj_add_event_cb(bx, [](lv_event_t *) { kb_cancel(); },
                        LV_EVENT_CLICKED, nullptr);

    lv_obj_t *kb = lv_keyboard_create(ov);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_set_size(kb, 800, 290);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(kb, CLR_TOPBAR, 0);
    lv_obj_add_event_cb(kb, [](lv_event_t *) { kb_ok(); },
                        LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(kb, [](lv_event_t *) { kb_cancel(); },
                        LV_EVENT_CANCEL, nullptr);
}

// ------------------------------------------------------------------ //
//  Globalni alarm popup                                               //
//  Pojavljuje se JEDNOM kad alarm postane aktivan.                    //
//  ZATVORI: sakriva popup, ne prikazuje ponovo za isti alarm.        //
//  RESET ALARM: šalje CMD_CLEAR_ALARM motoru + ide na manual ekran.  //
// ------------------------------------------------------------------ //
#include "data_model.h"
#include "espnow_hmi.h"

static bool      s_alarm_popup_shown   = false;
static bool      s_last_alarm_active   = false;
static lv_obj_t *s_alarm_popup_ov      = nullptr;

// ZATVORI — samo sakrij, ne prikazuj ponovo za ovaj alarm
static void alarm_popup_dismiss_cb(lv_event_t *)
{
    if (s_alarm_popup_ov) {
        lv_obj_del(s_alarm_popup_ov);
        s_alarm_popup_ov = nullptr;
        // s_alarm_popup_shown ostaje TRUE — ne prikazuj ponovo
    }
}

// RESET ALARM — pošalji reset motoru, idi na manual za homing
static void alarm_popup_reset_cb(lv_event_t *)
{
    espnow_hmi_send_clear_alarm();
    g_machine.alarm_active = false;
    g_machine.alarm_msg[0] = '\0';
    if (s_alarm_popup_ov) {
        lv_obj_del(s_alarm_popup_ov);
        s_alarm_popup_ov = nullptr;
    }
    s_alarm_popup_shown = false;
    ui_nav_go(SCREEN_MANUAL);    // korisnik vidi HOME dugme odmah
}

static void alarm_watcher_timer_cb(lv_timer_t *)
{
    bool now_active = g_machine.alarm_active;

    // Novi alarm: false → true = resetuj "shown" da se prikaže za novi alarm
    if (now_active && !s_last_alarm_active) {
        s_alarm_popup_shown = false;
    }
    s_last_alarm_active = now_active;

    // Prikaži popup (samo jednom po alarmu dok ga ne zatvori korisnik)
    if (now_active && !s_alarm_popup_shown) {
        s_alarm_popup_shown = true;

        // Taman overlay na lv_layer_top()
        lv_obj_t *ov = lv_obj_create(lv_layer_top());
        s_alarm_popup_ov = ov;
        lv_obj_set_size(ov, 800, 480);
        lv_obj_set_pos(ov, 0, 0);
        lv_obj_set_style_bg_color(ov, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(ov, LV_OPA_60, 0);
        lv_obj_set_style_border_width(ov, 0, 0);
        lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);

        // Dijaloški okvir — manji, ne zauzima cijeli ekran
        lv_obj_t *box = lv_obj_create(ov);
        lv_obj_set_size(box, 480, 210);
        lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(box, lv_color_make(0x2A, 0x08, 0x08), 0);
        lv_obj_set_style_border_color(box, CLR_RED, 0);
        lv_obj_set_style_border_width(box, 2, 0);
        lv_obj_set_style_radius(box, 12, 0);
        lv_obj_set_style_pad_all(box, 14, 0);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

        // Naslov
        lv_obj_t *ttl = lv_label_create(box);
        lv_label_set_text(ttl,
            g_language == LANG_SERBIAN
                ? LV_SYMBOL_WARNING "  GRESKA MOTORA"
                : LV_SYMBOL_WARNING "  MOTOR ERROR");
        lv_obj_set_style_text_font(ttl, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(ttl, CLR_RED, 0);
        lv_obj_align(ttl, LV_ALIGN_TOP_MID, 0, 0);

        // Poruka
        lv_obj_t *msg = lv_label_create(box);
        lv_label_set_text(msg, g_machine.alarm_msg[0]
                               ? g_machine.alarm_msg
                               : "Alarm na motoru.");
        lv_obj_set_style_text_font(msg, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(msg, CLR_TEXT, 0);
        lv_obj_set_width(msg, 450);
        lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
        lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 32);

        // Napomena
        lv_obj_t *note = lv_label_create(box);
        lv_label_set_text(note,
            g_language == LANG_SERBIAN
                ? "Nakon reseta alarma obavezno uraditi HOMING."
                : "After alarm reset, homing is required.");
        lv_obj_set_style_text_font(note, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(note, CLR_YELLOW, 0);
        lv_obj_set_width(note, 450);
        lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
        lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 68);

        // RESET ALARM — direktno ovdje, 1 tap
        lv_obj_t *b1 = ui_btn_create(box,
                                      g_language == LANG_SERBIAN
                                          ? LV_SYMBOL_OK " RESET ALARM"
                                          : LV_SYMBOL_OK " RESET ALARM",
                                      CLR_BTN_STOP, 210, 44);
        lv_obj_align(b1, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_add_event_cb(b1, alarm_popup_reset_cb, LV_EVENT_CLICKED, nullptr);

        // ZATVORI — samo skloni popup (ne resetuje alarm)
        lv_obj_t *b2 = ui_btn_create(box,
                                      g_language == LANG_SERBIAN
                                          ? LV_SYMBOL_CLOSE " ZATVORI"
                                          : LV_SYMBOL_CLOSE " CLOSE",
                                      CLR_BORDER, 160, 44);
        lv_obj_align(b2, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        lv_obj_add_event_cb(b2, alarm_popup_dismiss_cb, LV_EVENT_CLICKED, nullptr);

    } else if (!now_active && s_alarm_popup_ov) {
        // Motor sam ocistio alarm (npr. CMD_CLEAR_ALARM stigao) — zatvori auto
        lv_obj_del(s_alarm_popup_ov);
        s_alarm_popup_ov    = nullptr;
        s_alarm_popup_shown = false;
    }
}

void ui_alarm_watcher_init(void)
{
    s_alarm_popup_shown = false;
    s_last_alarm_active = false;
    s_alarm_popup_ov    = nullptr;
    lv_timer_create(alarm_watcher_timer_cb, 500, nullptr);
}
