#include "ui_programs.h"
#include "ui_common.h"
#include "storage.h"
#include "data_model.h"
#include "ui_strings.h"
#include <lvgl.h>
#include <cstdio>
#include <cstring>

static lv_obj_t *s_prog_list      = nullptr;
static lv_obj_t *s_steps_list     = nullptr;
static lv_obj_t *s_prog_statusbar = nullptr;
static int        s_sel_prog       = -1;

// Edit panel
static lv_obj_t *s_prog_name_lbl  = nullptr;
static lv_obj_t *s_prog_mat_lbl   = nullptr;
static lv_obj_t *s_mat_pick_ov    = nullptr;

static void prog_rebuild_list();
static void steps_rebuild_list();

// ---- Steps CRUD -------------------------------------------------
static void step_pos_done(float val, void *ud)
{
    if (s_sel_prog < 0) return;
    int step_idx = (int)(intptr_t)ud;
    Program &p = g_programs[s_sel_prog];
    if (step_idx < 0 || step_idx >= p.step_count) return;
    p.steps[step_idx].position_mm = val;
    steps_rebuild_list();
}

static void add_step_cb(lv_event_t *)
{
    if (s_sel_prog < 0) return;
    Program &p = g_programs[s_sel_prog];
    if (p.step_count >= MAX_STEPS) return;
    int new_idx = p.step_count;
    p.steps[new_idx].position_mm = 0.0f;
    p.step_count++;
    steps_rebuild_list();
    // Odmah otvori numpad za unos pozicije
    ui_numpad_open(g_language == LANG_SERBIAN ? "POZICIJA KORAKA (mm)" : "STEP POSITION (mm)", 0.0f,
                   step_pos_done, (void *)(intptr_t)new_idx);
}

static void del_step_cb(lv_event_t *e)
{
    if (s_sel_prog < 0) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    Program &p = g_programs[s_sel_prog];
    for (int i = idx; i < p.step_count - 1; i++)
        p.steps[i] = p.steps[i + 1];
    p.step_count--;
    steps_rebuild_list();
}

static void steps_rebuild_list()
{
    if (!s_steps_list) return;
    lv_obj_clean(s_steps_list);
    if (s_sel_prog < 0) return;
    Program &p = g_programs[s_sel_prog];

    for (int i = 0; i < p.step_count; i++) {
        lv_obj_t *row = lv_obj_create(s_steps_list);
        lv_obj_set_size(row, LV_PCT(100), 42);
        lv_obj_set_style_bg_color(row, CLR_CARD, 0);
        lv_obj_set_style_border_color(row, CLR_BORDER, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_pad_hor(row, 8, 0);
        lv_obj_set_style_pad_ver(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        // Pozicija kao primarna oznaka (broj koraka + mm)
        char buf[40];
        snprintf(buf, sizeof(buf), "%d.  %.3f mm", i + 1,
                    p.steps[i].position_mm);
        lv_obj_t *pl = lv_label_create(row);
        lv_label_set_text(pl, buf);
        lv_obj_set_style_text_font(pl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(pl, CLR_ACCENT, 0);
        lv_obj_align(pl, LV_ALIGN_LEFT_MID, 0, 0);

        // Edit dugme
        lv_obj_t *eb = ui_btn_create(row, LV_SYMBOL_EDIT, CLR_BORDER, 38, 30);
        lv_obj_align(eb, LV_ALIGN_RIGHT_MID, -44, 0);
        lv_obj_add_event_cb(eb, [](lv_event_t *e2) {
            int idx = (int)(intptr_t)lv_event_get_user_data(e2);
            if (s_sel_prog < 0) return;
            ui_numpad_open(g_language == LANG_SERBIAN ? "POZICIJA KORAKA (mm)" : "STEP POSITION (mm)",
                           g_programs[s_sel_prog].steps[idx].position_mm,
                           step_pos_done, (void *)(intptr_t)idx);
        }, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        // Del dugme
        lv_obj_t *db = ui_btn_create(row, LV_SYMBOL_TRASH, CLR_BTN_STOP, 38, 30);
        lv_obj_align(db, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(db, del_step_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
    }
}

// ---- Programs CRUD ----------------------------------------------
static void prog_select_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    s_sel_prog = idx;

    uint32_t cnt = lv_obj_get_child_cnt(s_prog_list);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *row = lv_obj_get_child(s_prog_list, i);
        lv_obj_set_style_bg_color(row,
            (int)i == idx ? lv_color_make(0x1E, 0x2D, 0x50) : CLR_CARD, 0);
    }

    if (s_prog_name_lbl)
        lv_label_set_text(s_prog_name_lbl, g_programs[idx].name);

    // Materijal naziv
    if (s_prog_mat_lbl) {
        int mi = g_programs[idx].material_idx;
        if (mi >= 0 && mi < g_material_count)
            lv_label_set_text(s_prog_mat_lbl, g_materials[mi].name);
        else
            lv_label_set_text(s_prog_mat_lbl,
                g_language == LANG_SERBIAN ? "— nije odabran —" : "— not selected —");
    }
    steps_rebuild_list();
}

static void prog_add_cb(lv_event_t *)
{
    if (g_program_count >= MAX_PROGRAMS) return;
    Program &p = g_programs[g_program_count];
    snprintf(p.name, NAME_LEN, "Program %d", g_program_count + 1);
    p.material_idx = -1;
    p.step_count   = 0;
    g_program_count++;
    prog_rebuild_list();
    s_sel_prog = g_program_count - 1;
    // Odmah ponudi promjenu naziva
    if (s_prog_name_lbl)
        lv_label_set_text(s_prog_name_lbl, p.name);
}

static void prog_del_cb(lv_event_t *)
{
    if (s_sel_prog < 0 || s_sel_prog >= g_program_count) return;
    for (int i = s_sel_prog; i < g_program_count - 1; i++)
        g_programs[i] = g_programs[i + 1];
    g_program_count--;
    s_sel_prog = -1;
    prog_rebuild_list();
    steps_rebuild_list();
}

static void prog_save_cb(lv_event_t *)
{
    storage_save_programs();
    if (s_prog_statusbar)
        ui_statusbar_set(s_prog_statusbar,
            g_language == LANG_SERBIAN ? LV_SYMBOL_OK "  SNIMLJENO" : LV_SYMBOL_OK "  SAVED",
            lv_color_make(0x69, 0xF0, 0xAE));
}

// Keyboard callback za naziv programa
static void prog_name_kb_done(const char *text, void *)
{
    if (s_sel_prog < 0) return;
    strncpy(g_programs[s_sel_prog].name, text, NAME_LEN - 1);
    g_programs[s_sel_prog].name[NAME_LEN - 1] = '\0';
    if (s_prog_name_lbl)
        lv_label_set_text(s_prog_name_lbl, text);
    prog_rebuild_list();
}

static void mat_pick_select_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (s_sel_prog < 0) return;
    Program &p = g_programs[s_sel_prog];
    p.material_idx = idx;
    if (s_prog_mat_lbl) {
        if (idx >= 0 && idx < g_material_count) lv_label_set_text(s_prog_mat_lbl, g_materials[idx].name);
        else lv_label_set_text(s_prog_mat_lbl,
            g_language == LANG_SERBIAN ? "— nije odabran —" : "— not selected —");
    }
    if (s_mat_pick_ov) {
        lv_obj_del(s_mat_pick_ov);
        s_mat_pick_ov = nullptr;
    }
}

static void mat_pick_open_cb(lv_event_t *)
{
    if (s_sel_prog < 0) return;
    if (s_mat_pick_ov) {
        lv_obj_del(s_mat_pick_ov);
        s_mat_pick_ov = nullptr;
    }

    lv_obj_t *ov = lv_obj_create(lv_scr_act());
    s_mat_pick_ov = ov;
    lv_obj_set_size(ov, 800, 480);
    lv_obj_set_pos(ov, 0, 0);
    lv_obj_set_style_bg_color(ov, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(ov, LV_OPA_70, 0);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *box = lv_obj_create(ov);
    lv_obj_set_size(box, 520, 380);
    lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(box, CLR_CARD, 0);
    lv_obj_set_style_border_color(box, CLR_BORDER, 0);
    lv_obj_set_style_radius(box, 12, 0);
    lv_obj_set_style_pad_all(box, 8, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ttl = lv_label_create(box);
    lv_label_set_text(ttl, g_language == LANG_SERBIAN ? "IZABERI MATERIJAL" : "SELECT MATERIAL");
    lv_obj_set_style_text_font(ttl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ttl, CLR_TEXT, 0);
    lv_obj_align(ttl, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t *lst = lv_obj_create(box);
    lv_obj_set_pos(lst, 10, 40);
    lv_obj_set_size(lst, 500, 286);
    lv_obj_set_style_bg_opa(lst, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(lst, 0, 0);
    lv_obj_set_style_pad_all(lst, 0, 0);
    lv_obj_set_style_pad_row(lst, 4, 0);
    lv_obj_set_flex_flow(lst, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(lst, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(lst, LV_SCROLLBAR_MODE_ACTIVE);

    lv_obj_t *none_btn = ui_btn_create(lst,
        g_language == LANG_SERBIAN ? "— NIJE ODABRAN —" : "— NOT SELECTED —",
        lv_color_make(0x2A, 0x3A, 0x5C), 500, 42);
    lv_obj_add_event_cb(none_btn, mat_pick_select_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-1);

    for (int i = 0; i < g_material_count; i++) {
        lv_obj_t *b = ui_btn_create(lst, g_materials[i].name, lv_color_make(0x2E, 0x4E, 0x72), 500, 42);
        lv_obj_add_event_cb(b, mat_pick_select_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    lv_obj_t *close_btn = ui_btn_create(box,
        g_language == LANG_SERBIAN ? LV_SYMBOL_CLOSE " ZATVORI" : LV_SYMBOL_CLOSE " CLOSE",
        CLR_BTN_STOP, 200, 40);
    lv_obj_align(close_btn, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_add_event_cb(close_btn, [](lv_event_t *) {
        if (s_mat_pick_ov) {
            lv_obj_del(s_mat_pick_ov);
            s_mat_pick_ov = nullptr;
        }
    }, LV_EVENT_CLICKED, nullptr);
}

static void prog_rebuild_list()
{
    if (!s_prog_list) return;
    lv_obj_clean(s_prog_list);
    for (int i = 0; i < g_program_count; i++) {
        lv_obj_t *row = lv_obj_create(s_prog_list);
        lv_obj_set_size(row, LV_PCT(100), 46);
        lv_obj_set_style_bg_color(row,
            i == s_sel_prog ? lv_color_make(0x1E, 0x2D, 0x50) : CLR_CARD, 0);
        lv_obj_set_style_border_color(row, CLR_BORDER, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_pad_hor(row, 10, 0);
        lv_obj_set_style_pad_ver(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(row, prog_select_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);

        lv_obj_t *nl = lv_label_create(row);
        lv_label_set_text(nl, g_programs[i].name);
        lv_obj_set_style_text_font(nl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(nl, CLR_TEXT, 0);
        lv_obj_align(nl, LV_ALIGN_LEFT_MID, 0, 0);

        char buf[24];
        snprintf(buf, sizeof(buf),
            g_language == LANG_SERBIAN ? "%d koraka" : "%d steps",
            g_programs[i].step_count);
        lv_obj_t *cl = lv_label_create(row);
        lv_label_set_text(cl, buf);
        lv_obj_set_style_text_font(cl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(cl, CLR_TEXT_DIM, 0);
        lv_obj_align(cl, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

void ui_programs_create(lv_obj_t *parent)
{
    s_prog_list = s_steps_list = nullptr;
    s_sel_prog  = -1;
    s_prog_name_lbl = s_prog_mat_lbl = nullptr;
    s_mat_pick_ov = nullptr;

    // ---- Leva lista programa (260px) --------------------------------
    lv_obj_t *prog_col = lv_obj_create(parent);
    lv_obj_set_pos(prog_col, 8, CONTENT_Y + 8);
    lv_obj_set_size(prog_col, 250, CONTENT_H - 10);
    lv_obj_set_style_bg_opa(prog_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(prog_col, 0, 0);
    lv_obj_set_style_pad_all(prog_col, 0, 0);
    lv_obj_set_flex_flow(prog_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(prog_col, LV_FLEX_ALIGN_START,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(prog_col, 4, 0);

    s_prog_list = lv_obj_create(prog_col);
    lv_obj_set_size(s_prog_list, 250, CONTENT_H - 60);
    lv_obj_set_style_bg_opa(s_prog_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_prog_list, 0, 0);
    lv_obj_set_style_pad_all(s_prog_list, 0, 0);
    lv_obj_set_style_pad_row(s_prog_list, 4, 0);
    lv_obj_set_flex_flow(s_prog_list, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *padd = ui_btn_create(prog_col,
                                    g_language == LANG_SERBIAN ? LV_SYMBOL_PLUS " PROGRAM" : LV_SYMBOL_PLUS " PROGRAM",
                                    CLR_BTN_GO, 220, 44);
    lv_obj_add_event_cb(padd, prog_add_cb, LV_EVENT_CLICKED, nullptr);

    // ---- Srednja kolona: edit programa (240px) -----------------------
    lv_obj_t *mid = ui_card_create(parent, 266, CONTENT_Y + 8, 240, CONTENT_H - 10,
                                    g_language == LANG_SERBIAN ? "INFO PROGRAMA" : "PROGRAM INFO");

    lv_obj_t *pnl = lv_label_create(mid);
    lv_label_set_text(pnl, g_language == LANG_SERBIAN ? "NAZIV:" : "NAME:");
    lv_obj_set_style_text_font(pnl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pnl, CLR_TEXT_DIM, 0);
    lv_obj_set_pos(pnl, 8, 36);

    s_prog_name_lbl = lv_label_create(mid);
    lv_label_set_text(s_prog_name_lbl, "—");
    lv_obj_set_style_text_font(s_prog_name_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_prog_name_lbl, CLR_ACCENT, 0);
    lv_obj_set_pos(s_prog_name_lbl, 8, 54);
    // Dugme za promjenu naziva programa
    lv_obj_t *pne = ui_btn_create(mid, LV_SYMBOL_EDIT,
                                   CLR_BORDER, 44, 28);
    lv_obj_set_pos(pne, 184, 52);
    lv_obj_add_event_cb(pne, [](lv_event_t *) {
        if (s_sel_prog < 0) return;
        ui_keyboard_open(g_language == LANG_SERBIAN ? "NAZIV PROGRAMA" : "PROGRAM NAME",
                         g_programs[s_sel_prog].name,
                         prog_name_kb_done, nullptr);
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *pml = lv_label_create(mid);
    lv_label_set_text(pml, g_language == LANG_SERBIAN ? "MATERIJAL:" : "MATERIAL:");
    lv_obj_set_style_text_font(pml, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pml, CLR_TEXT_DIM, 0);
    lv_obj_set_pos(pml, 8, 86);

    s_prog_mat_lbl = lv_label_create(mid);
    lv_label_set_text(s_prog_mat_lbl, "—");
    lv_obj_set_style_text_font(s_prog_mat_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_prog_mat_lbl, CLR_GREEN, 0);
    lv_obj_set_pos(s_prog_mat_lbl, 8, 104);

    lv_obj_t *mp = ui_btn_create(mid, LV_SYMBOL_EDIT,
                                  lv_color_make(0x2A, 0x3A, 0x5C), 44, 30);
    lv_obj_set_pos(mp, 184, 100);
    lv_obj_add_event_cb(mp, mat_pick_open_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *pdel = ui_btn_create(mid,
                                    g_language == LANG_SERBIAN ? LV_SYMBOL_TRASH " BRISANJE" : LV_SYMBOL_TRASH " DELETE",
                                    CLR_BTN_STOP, 210, 36);
    lv_obj_align(pdel, LV_ALIGN_BOTTOM_MID, 0, -48);
    lv_obj_add_event_cb(pdel, prog_del_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *psav = ui_btn_create(mid,
                                    g_language == LANG_SERBIAN ? LV_SYMBOL_SAVE " SNIMI" : LV_SYMBOL_SAVE " SAVE",
                                    CLR_BTN_GO, 210, 36);
    lv_obj_align(psav, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_add_event_cb(psav, prog_save_cb, LV_EVENT_CLICKED, nullptr);

    // ---- Desna kolona: koraci (280px) --------------------------------
    lv_obj_t *right = ui_card_create(parent, 514, CONTENT_Y + 8, 278, CONTENT_H - 10,
                                      g_language == LANG_SERBIAN ? "KORACI" : "STEPS");

    s_steps_list = lv_obj_create(right);
    lv_obj_set_pos(s_steps_list, 4, 34);
    lv_obj_set_size(s_steps_list, 268, CONTENT_H - 94);
    lv_obj_set_style_bg_opa(s_steps_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_steps_list, 0, 0);
    lv_obj_set_style_pad_all(s_steps_list, 0, 0);
    lv_obj_set_style_pad_row(s_steps_list, 4, 0);
    lv_obj_set_flex_flow(s_steps_list, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *sadd = ui_btn_create(right,
                                    g_language == LANG_SERBIAN ? LV_SYMBOL_PLUS " KORAK" : LV_SYMBOL_PLUS " STEP",
                                    CLR_BTN_GO, 250, 36);
    lv_obj_align(sadd, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_add_event_cb(sadd, add_step_cb, LV_EVENT_CLICKED, nullptr);

    s_prog_statusbar = ui_statusbar_create(parent);

    // Cleanup na brisanju ekrana
    lv_obj_add_event_cb(parent, [](lv_event_t *) {
        if (s_mat_pick_ov) {
            lv_obj_del(s_mat_pick_ov);
            s_mat_pick_ov = nullptr;
        }
        s_prog_list = s_steps_list = nullptr;
        s_prog_name_lbl = s_prog_mat_lbl = nullptr;
        s_prog_statusbar = nullptr;
        s_sel_prog = -1;
    }, LV_EVENT_DELETE, nullptr);

    prog_rebuild_list();
}
