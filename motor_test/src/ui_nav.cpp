#include "ui_nav.h"
#include "ui_common.h"
#include "ui_strings.h"

// Forward deklaracije kreatora ekrana (definisani u ui_*.cpp)
void ui_main_menu_create(lv_obj_t *parent);
void ui_manual_create(lv_obj_t *parent);
void ui_auto_create(lv_obj_t *parent);
void ui_programs_create(lv_obj_t *parent);
void ui_materials_create(lv_obj_t *parent);
void ui_settings_create(lv_obj_t *parent);
void ui_alarms_create(lv_obj_t *parent);

static ScreenId s_current = SCREEN_MAIN_MENU;

static const char *screen_title(ScreenId id)
{
    if (g_language == LANG_ENGLISH) {
        switch (id) {
            case SCREEN_MANUAL:    return "MANUAL MODE";
            case SCREEN_AUTO:      return "AUTO MODE";
            case SCREEN_PROGRAMS:  return "PROGRAMS";
            case SCREEN_MATERIALS: return "MATERIALS";
            case SCREEN_SETTINGS:  return "SETTINGS";
            case SCREEN_ALARMS:    return "ALARMS";
            default:               return "CNC BACK GAUGE";
        }
    }

    switch (id) {
        case SCREEN_MANUAL:    return "MANUELNI MOD";
        case SCREEN_AUTO:      return "AUTO MOD";
        case SCREEN_PROGRAMS:  return "PROGRAMI";
        case SCREEN_MATERIALS: return "MATERIJALI";
        case SCREEN_SETTINGS:  return "PODESAVANJA";
        case SCREEN_ALARMS:    return "ALARMI";
        default:               return "CNC BACK GAUGE";
    }
}

static void (*SCREEN_CREATORS[SCREEN_COUNT])(lv_obj_t *) = {
    ui_main_menu_create,
    ui_manual_create,
    ui_auto_create,
    ui_programs_create,
    ui_materials_create,
    ui_settings_create,
    ui_alarms_create,
};

void ui_nav_init()
{
    s_current = SCREEN_MAIN_MENU;
}

void ui_nav_go(ScreenId id)
{
    if (id < 0 || id >= SCREEN_COUNT) return;
    s_current = id;

    lv_obj_t *scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, lv_color_make(0x0D, 0x0F, 0x1A), 0);
    lv_obj_set_size(scr, 800, 480);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Topbar (zajednicki za sve osim main menija)
    if (id != SCREEN_MAIN_MENU) {
        ui_topbar_create(scr, screen_title(id), true);
    }

    // Sadrzaj ekrana
    SCREEN_CREATORS[id](scr);

    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, true);
}

void ui_nav_home()
{
    s_current = SCREEN_MAIN_MENU;

    lv_obj_t *scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, lv_color_make(0x0D, 0x0F, 0x1A), 0);
    lv_obj_set_size(scr, 800, 480);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    ui_main_menu_create(scr);

    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, true);
}

ScreenId ui_nav_current()
{
    return s_current;
}
