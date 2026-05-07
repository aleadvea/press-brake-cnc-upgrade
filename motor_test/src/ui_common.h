#pragma once
#include <lvgl.h>

// Boje teme
#define CLR_BG          lv_color_make(0x0D, 0x0F, 0x1A)
#define CLR_TOPBAR      lv_color_make(0x16, 0x1A, 0x2E)
#define CLR_CARD        lv_color_make(0x1C, 0x22, 0x3A)
#define CLR_BORDER      lv_color_make(0x2A, 0x3A, 0x5C)
#define CLR_TEXT        lv_color_make(0xCF, 0xD8, 0xFF)
#define CLR_TEXT_DIM    lv_color_make(0x70, 0x80, 0xA0)
#define CLR_ACCENT      lv_color_make(0x82, 0xB1, 0xFF)
#define CLR_GREEN       lv_color_make(0x69, 0xF0, 0xAE)
#define CLR_RED         lv_color_make(0xFF, 0x53, 0x52)
#define CLR_YELLOW      lv_color_make(0xFF, 0xD7, 0x40)
#define CLR_BTN_STOP    lv_color_make(0xC6, 0x28, 0x28)
#define CLR_BTN_GO      lv_color_make(0x2E, 0x7D, 0x32)

// Dimenzije
#define TOPBAR_H        50
#define STATUSBAR_H     36
#define CONTENT_Y       TOPBAR_H
#define CONTENT_H       (480 - TOPBAR_H - STATUSBAR_H)

// ------------------------------------------------------------------ //
//  Topbar: naslov (levo), HOME dugme (levo od naslova, samo kad !home),
//  sat (desno)                                                        //
// ------------------------------------------------------------------ //
void ui_topbar_create(lv_obj_t *parent, const char *title, bool show_home_btn);

// Azurira sat u topbaru (pozivati iz LVGL timera)
void ui_topbar_tick();

// ------------------------------------------------------------------ //
//  Status bar (dno ekrana)                                            //
// ------------------------------------------------------------------ //
lv_obj_t *ui_statusbar_create(lv_obj_t *parent);
void ui_statusbar_set(lv_obj_t *bar, const char *msg, lv_color_t color);

// ------------------------------------------------------------------ //
//  Numericka tastatura (modal za unos mm vrednosti)                   //
//  on_done(value) poziva se kad korisnik potvrdi                      //
// ------------------------------------------------------------------ //
typedef void (*numpad_done_cb)(float value, void *user_data);
void ui_numpad_open(const char *title, float current_val,
                    numpad_done_cb cb, void *user_data);
void ui_numpad_open_empty(const char *title,
                          numpad_done_cb cb, void *user_data);

// ------------------------------------------------------------------ //
//  Modal dijalog za potvrdu (da/ne)                                   //
// ------------------------------------------------------------------ //
typedef void (*confirm_cb)(bool confirmed, void *user_data);
void ui_confirm_open(const char *msg, confirm_cb cb, void *user_data);

// ------------------------------------------------------------------ //
//  Tastatura za unos teksta (naziv materijala, programa itd.)         //
// ------------------------------------------------------------------ //
typedef void (*keyboard_done_cb)(const char *text, void *user_data);
void ui_keyboard_open(const char *title, const char *current,
                      keyboard_done_cb cb, void *user_data);

// ------------------------------------------------------------------ //
//  Kartica (card) sa naslovnom linijom                                //
// ------------------------------------------------------------------ //
lv_obj_t *ui_card_create(lv_obj_t *parent, int x, int y, int w, int h,
                          const char *title);
lv_obj_t *ui_card_body(lv_obj_t *card);

// ------------------------------------------------------------------ //
//  Dugme standardnih stilova                                          //
// ------------------------------------------------------------------ //
lv_obj_t *ui_btn_create(lv_obj_t *parent, const char *label,
                         lv_color_t color, int w, int h);

// ------------------------------------------------------------------ //
//  Globalni alarm watcher — jednom pokrenuti odmah nakon lv_scr_load  //
//  Prikazuje popup na lv_layer_top() kad god alarm_active postane true //
// ------------------------------------------------------------------ //
void ui_alarm_watcher_init(void);
