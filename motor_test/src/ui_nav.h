#pragma once
#include <lvgl.h>

// ID-ovi ekrana
typedef enum {
    SCREEN_MAIN_MENU = 0,
    SCREEN_MANUAL,
    SCREEN_AUTO,
    SCREEN_PROGRAMS,
    SCREEN_MATERIALS,
    SCREEN_SETTINGS,
    SCREEN_ALARMS,
    SCREEN_COUNT
} ScreenId;

// Navigacija
void ui_nav_init();
void ui_nav_go(ScreenId id);
void ui_nav_home();          // vrati na SCREEN_MAIN_MENU
ScreenId ui_nav_current();
