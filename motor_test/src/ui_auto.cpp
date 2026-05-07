#include "ui_auto.h"
#include "ui_common.h"
#include "data_model.h"
#include "espnow_hmi.h"
#include "ui_strings.h"
#include <lvgl.h>
#include <cstdio>
#include <cstring>
#include <cmath>

enum AutoState {
    AS_IDLE = 0,
    AS_MOVING,
    AS_WAIT_BEND,
    AS_RETRACTING,
    AS_PAUSE,
};

static int       s_prog_idx      = -1;
static int       s_step          = -1;
static AutoState s_state         = AS_IDLE;
static int       s_part_count    = 0;
static uint32_t  s_pause_start   = 0;
static bool      s_bend_was_high = false;
static bool      s_move_started  = false;  // true kad motor stvarno krene
static float     s_target_pos_mm = 0.0f;
static bool      s_auto_owns_retract = false;
static bool      s_prev_retract_state = true;

static lv_obj_t   *s_prog_dd       = nullptr;
static lv_obj_t   *s_prog_name_lbl = nullptr;
static lv_obj_t   *s_mat_name_lbl  = nullptr;
static lv_obj_t   *s_step_info_lbl = nullptr;
static lv_obj_t   *s_parts_lbl     = nullptr;
static lv_obj_t   *s_cur_pos_lbl   = nullptr;
static lv_obj_t   *s_step_pos_lbl  = nullptr;
static lv_obj_t   *s_offset_lbl    = nullptr;
static lv_obj_t   *s_status_lbl    = nullptr;
static lv_obj_t   *s_steps_list    = nullptr;
static lv_obj_t   *s_start_btn     = nullptr;
static lv_obj_t   *s_stop_btn      = nullptr;
static lv_obj_t   *s_statusbar     = nullptr;
static lv_timer_t *s_timer         = nullptr;

static float get_material_offset()
{
    if (s_prog_idx < 0 || s_prog_idx >= g_program_count) return 0.0f;
    Program &p = g_programs[s_prog_idx];
    if (p.material_idx < 0 || p.material_idx >= g_material_count) return 0.0f;
    return g_materials[p.material_idx].offset_mm;
}

static void update_step_list()
{
    if (!s_steps_list) return;
    lv_obj_clean(s_steps_list);
    if (s_prog_idx < 0 || s_prog_idx >= g_program_count) return;
    Program &p = g_programs[s_prog_idx];
    float offset = get_material_offset();
    for (int i = 0; i < p.step_count; i++) {
        lv_obj_t *row = lv_obj_create(s_steps_list);
        lv_obj_set_size(row, LV_PCT(100), 44);
        lv_obj_set_style_bg_color(row,
            i == s_step ? lv_color_make(0x1B, 0x5E, 0x20) : CLR_CARD, 0);
        lv_obj_set_style_border_color(row, CLR_BORDER, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_pad_hor(row, 8, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        char buf[48];
        snprintf(buf, sizeof(buf),
            g_language == LANG_SERBIAN ? "KORAK %d" : "STEP %d",
            i + 1);
        lv_obj_t *nl = lv_label_create(row);
        lv_label_set_text(nl, buf);
        lv_obj_set_style_text_font(nl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(nl, CLR_TEXT_DIM, 0);
        lv_obj_align(nl, LV_ALIGN_LEFT_MID, 0, -10);

        snprintf(buf, sizeof(buf), "%.3f mm", p.steps[i].position_mm);
        lv_obj_t *pl = lv_label_create(row);
        lv_label_set_text(pl, buf);
        lv_obj_set_style_text_font(pl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(pl, CLR_ACCENT, 0);
        lv_obj_align(pl, LV_ALIGN_LEFT_MID, 0, 10);

        if (fabsf(offset) > 0.001f) {
            snprintf(buf, sizeof(buf), "%+.2f", offset);
            lv_obj_t *ol = lv_label_create(row);
            lv_label_set_text(ol, buf);
            lv_obj_set_style_text_font(ol, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(ol, CLR_YELLOW, 0);
            lv_obj_align(ol, LV_ALIGN_RIGHT_MID, 0, 10);
        }
    }
}

static void set_status(const char *txt, lv_color_t clr)
{
    if (s_status_lbl) {
        lv_label_set_text(s_status_lbl, txt);
        lv_obj_set_style_text_color(s_status_lbl, clr, 0);
    }
    if (s_statusbar) ui_statusbar_set(s_statusbar, txt, clr);
}

static void auto_take_retract_ownership()
{
    if (s_auto_owns_retract) return;
    s_prev_retract_state = g_retract_enabled;
    if (g_retract_enabled) {
        g_retract_enabled = false;
        espnow_hmi_send_sync();
    }
    s_auto_owns_retract = true;
}

static void auto_release_retract_ownership()
{
    if (!s_auto_owns_retract) return;
    g_retract_enabled = s_prev_retract_state;
    espnow_hmi_send_sync();
    s_auto_owns_retract = false;
}

static void update_info_labels()
{
    if (s_prog_idx < 0 || s_prog_idx >= g_program_count) return;
    Program &p = g_programs[s_prog_idx];
    float offset = get_material_offset();

    if (s_step_pos_lbl) {
        if (s_step >= 0 && s_step < p.step_count) {
            char buf[24];
            snprintf(buf, sizeof(buf), "%.3f mm", p.steps[s_step].position_mm);
            lv_label_set_text(s_step_pos_lbl, buf);
        } else {
            lv_label_set_text(s_step_pos_lbl, "-");
        }
    }
    if (s_offset_lbl) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%+.3f mm", offset);
        lv_label_set_text(s_offset_lbl, buf);
        lv_obj_set_style_text_color(s_offset_lbl,
            fabsf(offset) > 0.001f ? CLR_YELLOW : CLR_TEXT_DIM, 0);
    }
    if (s_step_info_lbl) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d / %d",
                 s_step >= 0 ? s_step + 1 : 0, p.step_count);
        lv_label_set_text(s_step_info_lbl, buf);
    }
    if (s_parts_lbl) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", s_part_count);
        lv_label_set_text(s_parts_lbl, buf);
    }
}

static void go_to_step(int step_idx)
{
    if (s_prog_idx < 0 || s_prog_idx >= g_program_count) return;
    Program &p = g_programs[s_prog_idx];
    if (step_idx < 0 || step_idx >= p.step_count) return;
    s_step         = step_idx;
    s_state        = AS_MOVING;
    s_move_started = false;   // reset: čekamo da motor stvarno krene

    s_target_pos_mm = p.steps[step_idx].position_mm + get_material_offset();
    espnow_hmi_send_move_abs(s_target_pos_mm, 0);

    char buf[64];
    snprintf(buf, sizeof(buf),
             g_language == LANG_SERBIAN ? "> KORAK %d  -> %.3f mm" : "> STEP %d  -> %.3f mm",
             step_idx + 1, p.steps[step_idx].position_mm);
    set_status(buf, lv_color_make(0x82, 0xB1, 0xFF));
    update_info_labels();
    update_step_list();
}

static void auto_timer_cb(lv_timer_t *)
{
    if (s_cur_pos_lbl) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%.3f mm", g_machine.position_mm);
        lv_label_set_text(s_cur_pos_lbl, buf);
    }

    if (s_state == AS_IDLE) return;
    if (s_prog_idx < 0 || s_prog_idx >= g_program_count) return;
    Program &p = g_programs[s_prog_idx];

    bool bend_high = g_machine.bend_sensor;

    switch (s_state) {

    case AS_MOVING:
        if (g_machine.motor_state == MSTATE_ALARM) {
            s_state = AS_IDLE;
            set_status(g_language == LANG_SERBIAN ? "! ALARM - program pauziran" : "! ALARM - program paused", CLR_RED);
            break;
        }
        // Postavi flag ÄŤim motor stvarno krene
        if (g_machine.motor_state != MSTATE_IDLE) s_move_started = true;
        // Ako smo vec na cilju i motor nije ni krenuo, odmah idi na cekanje savijanja.
        if (!s_move_started && g_machine.motor_state == MSTATE_IDLE) {
            if (fabsf(g_machine.position_mm - s_target_pos_mm) <= 0.02f) {
                s_bend_was_high = g_machine.bend_sensor;
                s_state = AS_WAIT_BEND;
                set_status(g_language == LANG_SERBIAN ? "|| Ceka savijanje..." : "|| Waiting for bend...", CLR_YELLOW);
                break;
            }
        }
        // Na cilj tek kad smo vidjeli pokret I motor je ponovo IDLE
        if (s_move_started && g_machine.motor_state == MSTATE_IDLE) {
            s_bend_was_high = g_machine.bend_sensor;  // sinkroniziraj stanje
            s_state = AS_WAIT_BEND;
            set_status(g_language == LANG_SERBIAN ? "|| Ceka savijanje..." : "|| Waiting for bend...", CLR_YELLOW);
        }
        break;

    case AS_WAIT_BEND:
        if (g_machine.motor_state == MSTATE_ALARM) {
            s_state = AS_IDLE;
            set_status(g_language == LANG_SERBIAN ? "! ALARM - program pauziran" : "! ALARM - program paused", CLR_RED);
            break;
        }
        if (!s_bend_was_high && bend_high) {
            float safe_max = g_settings.home_offset_mm - g_settings.home_clearance_mm;
            float ret_pos  = g_machine.position_mm + g_settings.retract_mm;
            if (ret_pos > safe_max) ret_pos = safe_max;
            float ret_spd  = g_settings.retract_speed_mmps > 0.0f
                             ? g_settings.retract_speed_mmps : 0.0f;
            espnow_hmi_send_move_abs(ret_pos, ret_spd);
            s_state = AS_RETRACTING;
            set_status(g_language == LANG_SERBIAN ? "^ Retract - ceka kraj savijanja..." : "^ Retract - waiting bend end...", CLR_ACCENT);
        }
        s_bend_was_high = bend_high;
        break;

    case AS_RETRACTING:
        if (s_bend_was_high && !bend_high) {
            s_state       = AS_PAUSE;
            s_pause_start = lv_tick_get();
            char buf[48];
            snprintf(buf, sizeof(buf),
                     "@ Pauza %.0fs...", g_settings.auto_retract_pause_s);
            set_status(buf, CLR_YELLOW);
        }
        s_bend_was_high = bend_high;
        break;

    case AS_PAUSE: {
        uint32_t elapsed_ms = lv_tick_elaps(s_pause_start);
        uint32_t pause_ms   = (uint32_t)(g_settings.auto_retract_pause_s * 1000.0f);
        if (elapsed_ms >= pause_ms) {
            int next = s_step + 1;
            if (next >= p.step_count) {
                s_part_count++;
                if (s_parts_lbl) {
                    char buf[12];
                    snprintf(buf, sizeof(buf), "%d", s_part_count);
                    lv_label_set_text(s_parts_lbl, buf);
                }
                set_status(g_language == LANG_SERBIAN ? "OK Ciklus zavrsen! Ide na korak 1..." : "OK Cycle complete! Going to step 1...", CLR_GREEN);
                go_to_step(0);
            } else {
                go_to_step(next);
            }
        } else {
            float remaining = g_settings.auto_retract_pause_s - (float)elapsed_ms / 1000.0f;
            char buf[48];
            snprintf(buf, sizeof(buf),
                     "@ Pauza %.1fs...", remaining > 0.0f ? remaining : 0.0f);
            set_status(buf, CLR_YELLOW);
        }
        break;
    }

    default: break;
    }
}

static void select_program(int idx)
{
    if (idx < 0 || idx >= g_program_count) return;
    s_prog_idx = idx;
    s_step     = -1;
    s_state    = AS_IDLE;
    Program &p = g_programs[idx];

    if (s_prog_name_lbl) lv_label_set_text(s_prog_name_lbl, p.name);
    if (s_mat_name_lbl) {
        lv_label_set_text(s_mat_name_lbl,
            (p.material_idx >= 0 && p.material_idx < g_material_count)
            ? g_materials[p.material_idx].name : "-");
    }
    if (s_step_info_lbl) {
        char buf[16];
        snprintf(buf, sizeof(buf), "0 / %d", p.step_count);
        lv_label_set_text(s_step_info_lbl, buf);
    }
    if (s_offset_lbl) {
        float off = get_material_offset();
        char buf[24];
        snprintf(buf, sizeof(buf), "%+.3f mm", off);
        lv_label_set_text(s_offset_lbl, buf);
        lv_obj_set_style_text_color(s_offset_lbl,
            fabsf(off) > 0.001f ? CLR_YELLOW : CLR_TEXT_DIM, 0);
    }
    if (s_step_pos_lbl) lv_label_set_text(s_step_pos_lbl, "-");
    set_status(g_language == LANG_SERBIAN ? "|| Odabran program - pritisni START" : "|| Program selected - press START", CLR_TEXT_DIM);
    update_step_list();
}

static void prog_dd_cb(lv_event_t *)
{
    if (!s_prog_dd) return;
    select_program((int)lv_dropdown_get_selected(s_prog_dd));
}

static void start_cb(lv_event_t *)
{
    if (s_prog_idx < 0) return;
    if (!g_machine.is_homed) {
        set_status(g_language == LANG_SERBIAN ? "! Motor nije referiran! Uradi HOMING." : "! Motor is not homed! Run HOMING.", CLR_RED);
        return;
    }
    auto_take_retract_ownership();
    s_bend_was_high = g_machine.bend_sensor;
    go_to_step(0);
}

static void stop_cb(lv_event_t *)
{
    s_state = AS_IDLE;
    espnow_hmi_send_stop();
    auto_release_retract_ownership();
    set_status(g_language == LANG_SERBIAN ? "[] ZAUSTAVLJENO" : "[] STOPPED", CLR_YELLOW);
}

static void step_prev_cb(lv_event_t *)
{
    if (s_state == AS_MOVING) return;
    if (s_prog_idx < 0) return;
    int prev = (s_step <= 0) ? 0 : s_step - 1;
    s_bend_was_high = g_machine.bend_sensor;
    go_to_step(prev);
}

static void step_next_cb(lv_event_t *)
{
    if (s_state == AS_MOVING) return;
    if (s_prog_idx < 0 || s_prog_idx >= g_program_count) return;
    Program &p = g_programs[s_prog_idx];
    int next = (s_step + 1 >= p.step_count) ? 0 : s_step + 1;
    s_bend_was_high = g_machine.bend_sensor;
    go_to_step(next);
}

void ui_auto_create(lv_obj_t *parent)
{
    // ---------------------------------------------------------------
    // Ekran 800x480 | TOPBAR=50 | STATUSBAR=36 | CONTENT_H=394
    //
    //  LEFT  col: x=8,   w=382  (PROGRAM + POZICIJA + STATUS)
    //  RIGHT col: x=398, w=394  (KORACI scroll + KONTROLE)
    // ---------------------------------------------------------------

    s_prog_idx      = -1;
    s_step          = -1;
    s_state         = AS_IDLE;
    s_move_started  = false;
    s_bend_was_high = false;
    s_auto_owns_retract = false;
    s_prev_retract_state = g_retract_enabled;
    s_prog_dd       = nullptr;
    s_prog_name_lbl = s_mat_name_lbl = nullptr;
    s_step_info_lbl = s_parts_lbl    = nullptr;
    s_cur_pos_lbl   = s_step_pos_lbl = s_offset_lbl = nullptr;
    s_status_lbl    = s_steps_list   = nullptr;
    s_start_btn     = s_stop_btn     = s_statusbar = nullptr;
    s_timer         = nullptr;

    // ================================================================
    // LEFT â€” PROGRAM card  (h=78)
    //   header 28px | dropdown y=30 h=32 | mat y=64
    // ================================================================
    lv_obj_t *prog_card = ui_card_create(parent, 8, CONTENT_Y + 4, 382, 78,
        g_language == LANG_SERBIAN ? "PROGRAM" : "PROGRAM");

    s_prog_dd = lv_dropdown_create(prog_card);
    lv_obj_set_size(s_prog_dd, 364, 32);
    lv_obj_set_pos(s_prog_dd, 9, 30);
    lv_obj_set_style_text_font(s_prog_dd, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(s_prog_dd, CLR_BORDER, 0);
    lv_obj_set_style_text_color(s_prog_dd, CLR_TEXT, 0);
    if (g_program_count > 0) {
        char opts[MAX_PROGRAMS * (NAME_LEN + 1)];
        opts[0] = '\0';
        for (int i = 0; i < g_program_count; i++) {
            if (i > 0) strcat(opts, "\n");
            strcat(opts, g_programs[i].name);
        }
        lv_dropdown_set_options(s_prog_dd, opts);
    } else {
        lv_dropdown_set_options(s_prog_dd, g_language == LANG_SERBIAN ? "-- nema programa --" : "-- no programs --");
    }
    lv_obj_add_event_cb(s_prog_dd, prog_dd_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    s_prog_name_lbl = nullptr;  // nije prikazano odvojeno (dropdown vec pokazuje ime)

    // ================================================================
    // LEFT â€” POZICIJA card  (y=86, h=CONTENT_H-90=304)
    //   Velik prikaz zadane pozicije koraka + offset
    //   Manja stvarna pozicija motora ispod
    //   Zatim KORAK/KOMADI te STATUS tekst
    // ================================================================
    lv_obj_t *pos_card = ui_card_create(parent, 8, CONTENT_Y + 86, 382, CONTENT_H - 90,
        g_language == LANG_SERBIAN ? "POZICIJA" : "POSITION");

    // --- ZADATA (step_pos) - veliki font ---
    {
        lv_obj_t *lbl = lv_label_create(pos_card);
        lv_label_set_text(lbl, g_language == LANG_SERBIAN ? "ZADATA:" : "TARGET:");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, CLR_TEXT_DIM, 0);
        lv_obj_set_pos(lbl, 9, 32);
    }
    s_step_pos_lbl = lv_label_create(pos_card);
    lv_label_set_text(s_step_pos_lbl, "---.--- mm");
    lv_obj_set_style_text_font(s_step_pos_lbl, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(s_step_pos_lbl, CLR_ACCENT, 0);
    lv_obj_set_pos(s_step_pos_lbl, 9, 48);

    // OFFSET â€” desno od zadata (y=48, align right)
    s_offset_lbl = lv_label_create(pos_card);
    lv_label_set_text(s_offset_lbl, "+0.000");
    lv_obj_set_style_text_font(s_offset_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(s_offset_lbl, CLR_TEXT_DIM, 0);
    lv_obj_align(s_offset_lbl, LV_ALIGN_TOP_RIGHT, -9, 50);

    // --- MOTOR (stvarna pozicija amotora) ---
    {
        lv_obj_t *lbl = lv_label_create(pos_card);
        lv_label_set_text(lbl, "MOTOR:");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, CLR_TEXT_DIM, 0);
        lv_obj_set_pos(lbl, 9, 104);
    }
    s_cur_pos_lbl = lv_label_create(pos_card);
    lv_label_set_text(s_cur_pos_lbl, "0.000 mm");
    lv_obj_set_style_text_font(s_cur_pos_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_cur_pos_lbl, CLR_GREEN, 0);
    lv_obj_set_pos(s_cur_pos_lbl, 62, 100);

    // --- KORAK + KOMADI ---
    {
        lv_obj_t *lbl = lv_label_create(pos_card);
        lv_label_set_text(lbl, g_language == LANG_SERBIAN ? "KORAK:" : "STEP:");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, CLR_TEXT_DIM, 0);
        lv_obj_set_pos(lbl, 9, 144);
    }
    s_step_info_lbl = lv_label_create(pos_card);
    lv_label_set_text(s_step_info_lbl, "0 / 0");
    lv_obj_set_style_text_font(s_step_info_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_step_info_lbl, CLR_YELLOW, 0);
    lv_obj_set_pos(s_step_info_lbl, 58, 141);

    {
        lv_obj_t *lbl = lv_label_create(pos_card);
        lv_label_set_text(lbl, g_language == LANG_SERBIAN ? "KOMADI:" : "PARTS:");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, CLR_TEXT_DIM, 0);
        lv_obj_set_pos(lbl, 200, 144);
    }
    s_parts_lbl = lv_label_create(pos_card);
    lv_label_set_text(s_parts_lbl, "0");
    lv_obj_set_style_text_font(s_parts_lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_parts_lbl, CLR_GREEN, 0);
    lv_obj_set_pos(s_parts_lbl, 260, 138);

    // --- STATUS tekst (wraps) ---
    {
        // tanki separator
        lv_obj_t *sep = lv_obj_create(pos_card);
        lv_obj_set_size(sep, 360, 1);
        lv_obj_set_pos(sep, 9, 178);
        lv_obj_set_style_bg_color(sep, CLR_BORDER, 0);
        lv_obj_set_style_border_width(sep, 0, 0);
    }
    s_status_lbl = lv_label_create(pos_card);
    lv_label_set_text(s_status_lbl,
        g_language == LANG_SERBIAN ? "Odaberi program i pritisni START" : "Select program and press START");
    lv_obj_set_style_text_font(s_status_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_status_lbl, CLR_TEXT_DIM, 0);
    lv_obj_set_pos(s_status_lbl, 9, 184);
    lv_label_set_long_mode(s_status_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_status_lbl, 364);

    // ================================================================
    // RIGHT â€” KORACI PROGRAMA card (x=398, y=4, w=394, h=222, scroll)
    // ================================================================
    lv_obj_t *steps_card = ui_card_create(parent, 398, CONTENT_Y + 4,
                                           394, 222, g_language == LANG_SERBIAN ? "KORACI PROGRAMA" : "PROGRAM STEPS");
    s_steps_list = lv_obj_create(steps_card);
    lv_obj_set_pos(s_steps_list, 4, 30);
    lv_obj_set_size(s_steps_list, 384, 188);
    lv_obj_set_style_bg_opa(s_steps_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_steps_list, 0, 0);
    lv_obj_set_style_pad_all(s_steps_list, 0, 0);
    lv_obj_set_style_pad_row(s_steps_list, 4, 0);
    lv_obj_set_flex_flow(s_steps_list, LV_FLEX_FLOW_COLUMN);
    // Scroll je automatski kad sadrzaj prelazi visinu

    // ================================================================
    // RIGHT â€” KONTROLE card (x=398, y=230, w=394, h=CONTENT_H-232=162)
    //   header 28px
    //   mat label y=30
    //   [START][STOP]   y=48  h=46
    //   [KORAK-][KORAK+] y=100 h=44
    //   KORAK+KOMADI info y=152
    // ================================================================
    lv_obj_t *ctrl_card = ui_card_create(parent, 398, CONTENT_Y + 230,
                                          394, CONTENT_H - 232, g_language == LANG_SERBIAN ? "KONTROLE" : "CONTROLS");

    // Materijal (koji je aktivan)
    {
        lv_obj_t *lbl = lv_label_create(ctrl_card);
        lv_label_set_text(lbl, g_language == LANG_SERBIAN ? "MAT:" : "MAT:");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, CLR_TEXT_DIM, 0);
        lv_obj_set_pos(lbl, 9, 30);
    }
    s_mat_name_lbl = lv_label_create(ctrl_card);
    lv_label_set_text(s_mat_name_lbl, "--");
    lv_obj_set_style_text_font(s_mat_name_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_mat_name_lbl, CLR_GREEN, 0);
    lv_obj_set_pos(s_mat_name_lbl, 40, 30);

    // Button row 1: START | STOP  (w=183 each, gap=6, lpad=9)
    s_start_btn = ui_btn_create(ctrl_card, g_language == LANG_SERBIAN ? ">  START" : ">  START", CLR_BTN_GO, 183, 48);
    lv_obj_set_pos(s_start_btn, 9, 48);
    lv_obj_add_event_cb(s_start_btn, start_cb, LV_EVENT_CLICKED, nullptr);

    s_stop_btn = ui_btn_create(ctrl_card, "[]  STOP", CLR_BTN_STOP, 183, 48);
    lv_obj_set_pos(s_stop_btn, 198, 48);
    lv_obj_add_event_cb(s_stop_btn, stop_cb, LV_EVENT_CLICKED, nullptr);

    // Button row 2: KORAK- | KORAK+
    lv_obj_t *btn_prev = ui_btn_create(ctrl_card,
        g_language == LANG_SERBIAN ? "< KORAK -" : "< STEP -", lv_color_make(0x37, 0x47, 0x4F), 183, 44);
    lv_obj_set_pos(btn_prev, 9, 102);
    lv_obj_add_event_cb(btn_prev, step_prev_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *btn_next = ui_btn_create(ctrl_card,
        g_language == LANG_SERBIAN ? "KORAK + >" : "STEP + >", lv_color_make(0x1A, 0x56, 0x96), 183, 44);
    lv_obj_set_pos(btn_next, 198, 102);
    lv_obj_add_event_cb(btn_next, step_next_cb, LV_EVENT_CLICKED, nullptr);

    // ================================================================
    s_statusbar = ui_statusbar_create(parent);
    s_timer     = lv_timer_create(auto_timer_cb, 50, nullptr);

    lv_obj_add_event_cb(parent, [](lv_event_t *) {
        auto_release_retract_ownership();
        if (s_timer) { lv_timer_del(s_timer); s_timer = nullptr; }
        s_prog_dd       = nullptr;
        s_prog_name_lbl = s_mat_name_lbl = nullptr;
        s_step_info_lbl = s_parts_lbl    = nullptr;
        s_cur_pos_lbl   = s_step_pos_lbl = s_offset_lbl = nullptr;
        s_status_lbl    = s_steps_list   = nullptr;
        s_start_btn     = s_stop_btn     = s_statusbar = nullptr;
        s_state = AS_IDLE;
    }, LV_EVENT_DELETE, nullptr);

    if (g_program_count > 0) select_program(0);
}