#include "ui_materials.h"
#include "ui_common.h"
#include "storage.h"
#include "data_model.h"
#include "ui_strings.h"
#include <lvgl.h>
#include <cstdio>
#include <cstring>

static lv_obj_t *s_mat_list      = nullptr;
static lv_obj_t *s_mat_statusbar = nullptr;
static int        s_selected_mat = -1;

// Forward
static void mat_rebuild_list();

// ---- Edit forma (desna strana) -----------------------------------
static lv_obj_t *s_name_lbl      = nullptr;
static lv_obj_t *s_thick_lbl     = nullptr;
static lv_obj_t *s_offset_lbl    = nullptr;
static int        s_edit_idx      = -1;

static void update_edit_panel()
{
    if (s_edit_idx < 0 || s_edit_idx >= g_material_count) return;
    Material &m = g_materials[s_edit_idx];
    if (s_name_lbl)   lv_label_set_text(s_name_lbl, m.name);
    char buf[32];
    if (s_thick_lbl) {
        snprintf(buf, sizeof(buf), "%.3f mm", m.thickness_mm);
        lv_label_set_text(s_thick_lbl, buf);
    }
    if (s_offset_lbl) {
        snprintf(buf, sizeof(buf), "%.3f mm", m.offset_mm);
        lv_label_set_text(s_offset_lbl, buf);
    }
}

// Numpad done callbacks
static void thick_done(float val, void *) {
    if (s_edit_idx < 0) return;
    g_materials[s_edit_idx].thickness_mm = val;
    update_edit_panel();
}
static void offset_done(float val, void *) {
    if (s_edit_idx < 0) return;
    g_materials[s_edit_idx].offset_mm = val;
    update_edit_panel();
}

static void name_kb_done(const char *text, void *) {
    if (s_edit_idx < 0) return;
    strncpy(g_materials[s_edit_idx].name, text, NAME_LEN - 1);
    g_materials[s_edit_idx].name[NAME_LEN - 1] = '\0';
    update_edit_panel();
    mat_rebuild_list();
}

static void mat_select_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    s_selected_mat = idx;
    s_edit_idx     = idx;

    // Obelezi selektovani
    uint32_t cnt = lv_obj_get_child_cnt(s_mat_list);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *row = lv_obj_get_child(s_mat_list, i);
        lv_obj_set_style_bg_color(row,
            (int)i == idx ? lv_color_make(0x1E, 0x2D, 0x50) : CLR_CARD, 0);
    }
    update_edit_panel();
}

static void mat_add_cb(lv_event_t *)
{
    if (g_material_count >= MAX_MATERIALS) return;
    Material &m = g_materials[g_material_count];
    snprintf(m.name, NAME_LEN,
             g_language == LANG_SERBIAN ? "Materijal %d" : "Material %d",
             g_material_count + 1);
    m.thickness_mm = 1.0f;
    m.offset_mm    = 0.0f;
    g_material_count++;
    mat_rebuild_list();
    s_edit_idx = g_material_count - 1;
    update_edit_panel();
}

static void mat_delete_cb(lv_event_t *)
{
    if (s_edit_idx < 0 || s_edit_idx >= g_material_count) return;
    for (int i = s_edit_idx; i < g_material_count - 1; i++)
        g_materials[i] = g_materials[i + 1];
    g_material_count--;
    s_edit_idx = -1;
    mat_rebuild_list();
}

static void mat_save_cb(lv_event_t *)
{
    storage_save_materials();
    if (s_mat_statusbar)
        ui_statusbar_set(s_mat_statusbar,
            g_language == LANG_SERBIAN ? LV_SYMBOL_OK "  SNIMLJENO" : LV_SYMBOL_OK "  SAVED",
            lv_color_make(0x69, 0xF0, 0xAE));
}

static void mat_rebuild_list()
{
    if (!s_mat_list) return;
    lv_obj_clean(s_mat_list);
    for (int i = 0; i < g_material_count; i++) {
        lv_obj_t *row = lv_obj_create(s_mat_list);
        lv_obj_set_size(row, LV_PCT(100), 46);
        lv_obj_set_style_bg_color(row,
            i == s_edit_idx ? lv_color_make(0x1E, 0x2D, 0x50) : CLR_CARD, 0);
        lv_obj_set_style_border_color(row, CLR_BORDER, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_pad_hor(row, 10, 0);
        lv_obj_set_style_pad_ver(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(row, mat_select_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);

        lv_obj_t *nl = lv_label_create(row);
        lv_label_set_text(nl, g_materials[i].name);
        lv_obj_set_style_text_font(nl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(nl, CLR_TEXT, 0);
        lv_obj_align(nl, LV_ALIGN_LEFT_MID, 0, 0);

        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f mm | +%.3f",
                    g_materials[i].thickness_mm,
                    g_materials[i].offset_mm);
        lv_obj_t *dl = lv_label_create(row);
        lv_label_set_text(dl, buf);
        lv_obj_set_style_text_font(dl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(dl, CLR_TEXT_DIM, 0);
        lv_obj_align(dl, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

void ui_materials_create(lv_obj_t *parent)
{
    s_mat_list   = nullptr;
    s_edit_idx   = -1;
    s_selected_mat = -1;
    s_name_lbl = s_thick_lbl = s_offset_lbl = nullptr;

    // ---- Leva strana: lista (470px) ---------------------------------
    lv_obj_t *left = lv_obj_create(parent);
    lv_obj_set_pos(left, 8, CONTENT_Y + 8);
    lv_obj_set_size(left, 460, CONTENT_H - 10);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 0, 0);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(left, 0, 0);

    // Scrollable lista
    s_mat_list = lv_obj_create(left);
    lv_obj_set_size(s_mat_list, 460, CONTENT_H - 60);
    lv_obj_set_style_bg_opa(s_mat_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_mat_list, 0, 0);
    lv_obj_set_style_pad_all(s_mat_list, 0, 0);
    lv_obj_set_style_pad_row(s_mat_list, 4, 0);
    lv_obj_set_flex_flow(s_mat_list, LV_FLEX_FLOW_COLUMN);

    // + ADD dugme ispod liste
    lv_obj_t *add_btn = ui_btn_create(left, g_language == LANG_SERBIAN ? LV_SYMBOL_PLUS " DODAJ" : LV_SYMBOL_PLUS " ADD",
                                       CLR_BTN_GO, 200, 44);
    lv_obj_add_event_cb(add_btn, mat_add_cb, LV_EVENT_CLICKED, nullptr);

    // ---- Desna strana: edit forma (310px) ---------------------------
    lv_obj_t *edit_card = ui_card_create(parent, 476, CONTENT_Y + 8, 316, CONTENT_H - 10,
                                          g_language == LANG_SERBIAN ? "UREDI MATERIJAL" : "EDIT MATERIAL");

    // Naziv
    lv_obj_t *nl = lv_label_create(edit_card);
    lv_label_set_text(nl, g_language == LANG_SERBIAN ? "NAZIV:" : "NAME:");
    lv_obj_set_style_text_font(nl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(nl, CLR_TEXT_DIM, 0);
    lv_obj_set_pos(nl, 8, 36);

    s_name_lbl = lv_label_create(edit_card);
    lv_label_set_text(s_name_lbl, "—");
    lv_obj_set_style_text_font(s_name_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_name_lbl, CLR_ACCENT, 0);
    lv_obj_set_pos(s_name_lbl, 8, 54);
    // Dugme za promjenu naziva
    lv_obj_t *ne = ui_btn_create(edit_card, LV_SYMBOL_EDIT,
                                  CLR_BORDER, 50, 30);
    lv_obj_set_pos(ne, 256, 50);
    lv_obj_add_event_cb(ne, [](lv_event_t *) {
        if (s_edit_idx < 0) return;
        ui_keyboard_open(g_language == LANG_SERBIAN ? "NAZIV MATERIJALA" : "MATERIAL NAME",
                         g_materials[s_edit_idx].name,
                         name_kb_done, nullptr);
    }, LV_EVENT_CLICKED, nullptr);
    // Debljina
    lv_obj_t *tl = lv_label_create(edit_card);
    lv_label_set_text(tl, g_language == LANG_SERBIAN ? "DEBLJINA:" : "THICKNESS:");
    lv_obj_set_style_text_font(tl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tl, CLR_TEXT_DIM, 0);
    lv_obj_set_pos(tl, 8, 86);

    s_thick_lbl = lv_label_create(edit_card);
    lv_label_set_text(s_thick_lbl, "—");
    lv_obj_set_style_text_font(s_thick_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_thick_lbl, CLR_GREEN, 0);
    lv_obj_set_pos(s_thick_lbl, 8, 104);

    lv_obj_t *te = ui_btn_create(edit_card, LV_SYMBOL_EDIT, CLR_BORDER, 50, 34);
    lv_obj_set_pos(te, 250, 100);
    lv_obj_add_event_cb(te, [](lv_event_t *) {
        if (s_edit_idx < 0) return;
        ui_numpad_open(g_language == LANG_SERBIAN ? "DEBLJINA (mm)" : "THICKNESS (mm)",
                       g_materials[s_edit_idx].thickness_mm,
                       thick_done, nullptr);
    }, LV_EVENT_CLICKED, nullptr);

    // Offset
    lv_obj_t *ol = lv_label_create(edit_card);
    lv_label_set_text(ol, "OFFSET:");
    lv_obj_set_style_text_font(ol, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ol, CLR_TEXT_DIM, 0);
    lv_obj_set_pos(ol, 8, 150);

    s_offset_lbl = lv_label_create(edit_card);
    lv_label_set_text(s_offset_lbl, "—");
    lv_obj_set_style_text_font(s_offset_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_offset_lbl, CLR_YELLOW, 0);
    lv_obj_set_pos(s_offset_lbl, 8, 168);

    lv_obj_t *oe = ui_btn_create(edit_card, LV_SYMBOL_EDIT, CLR_BORDER, 50, 34);
    lv_obj_set_pos(oe, 250, 164);
    lv_obj_add_event_cb(oe, [](lv_event_t *) {
        if (s_edit_idx < 0) return;
        ui_numpad_open("OFFSET (mm)",
                       g_materials[s_edit_idx].offset_mm,
                       offset_done, nullptr);
    }, LV_EVENT_CLICKED, nullptr);

    // Delete dugme
    lv_obj_t *del_btn = ui_btn_create(edit_card, g_language == LANG_SERBIAN ? LV_SYMBOL_TRASH " BRISANJE" : LV_SYMBOL_TRASH " DELETE",
                                       CLR_BTN_STOP, 280, 40);
    lv_obj_align(del_btn, LV_ALIGN_BOTTOM_MID, 0, -54);
    lv_obj_add_event_cb(del_btn, mat_delete_cb, LV_EVENT_CLICKED, nullptr);

    // Snimi dugme
    lv_obj_t *sav = ui_btn_create(edit_card, g_language == LANG_SERBIAN ? LV_SYMBOL_SAVE " SNIMI" : LV_SYMBOL_SAVE " SAVE",
                                   CLR_BTN_GO, 280, 40);
    lv_obj_align(sav, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_add_event_cb(sav, mat_save_cb, LV_EVENT_CLICKED, nullptr);

    // Status bar
    s_mat_statusbar = ui_statusbar_create(parent);

    // Cleanup na brisanju ekrana
    lv_obj_add_event_cb(parent, [](lv_event_t *) {
        s_mat_list = s_mat_statusbar = nullptr;
        s_name_lbl = s_thick_lbl = s_offset_lbl = nullptr;
        s_edit_idx = s_selected_mat = -1;
    }, LV_EVENT_DELETE, nullptr);

    // Popuni listu
    mat_rebuild_list();
}
