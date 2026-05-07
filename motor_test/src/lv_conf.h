/**
 * lv_conf.h – LVGL v8.4 konfiguracija za Waveshare ESP32-S3 7" LCD
 */

#if 1  /* MORA biti 1 za LVGL v8 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*==================== COLOR ====================*/
#define LV_COLOR_DEPTH          16   /* RGB565 */
#define LV_COLOR_16_SWAP        0    /* 0 za RGB LCD (bez swap) */

/*==================== MEMORY ====================*/
/* Route LVGL's 256 KB static heap to PSRAM so it does NOT live in DRAM BSS */
#define LV_MEM_CUSTOM           1
#if LV_MEM_CUSTOM == 0
#define LV_MEM_SIZE             (256 * 1024U)
#else
#define LV_MEM_CUSTOM_INCLUDE   "lvgl_psram_alloc.h"
#define LV_MEM_CUSTOM_ALLOC     lv_psram_malloc
#define LV_MEM_CUSTOM_REALLOC   lv_psram_realloc
#define LV_MEM_CUSTOM_FREE      free
#endif
#define LV_MEM_BUF_MAX_NUM      16
#define LV_MEMCPY_MEMSET_STD    1

/*==================== TASK ====================*/
#define LV_TICK_CUSTOM          0

/*==================== FEATURES ====================*/
#define LV_USE_LOG              0
#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1

/*==================== RENDERING ====================*/
#define LV_DISP_DEF_REFR_PERIOD  30
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_ATTRIBUTE_FAST_MEM   IRAM_ATTR

/*==================== FONTS ====================*/
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_18   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_22   0
#define LV_FONT_MONTSERRAT_24   1
#define LV_FONT_MONTSERRAT_26   0
#define LV_FONT_MONTSERRAT_28   1
#define LV_FONT_MONTSERRAT_30   0
#define LV_FONT_MONTSERRAT_32   1
#define LV_FONT_MONTSERRAT_36   1
#define LV_FONT_MONTSERRAT_38   0
#define LV_FONT_MONTSERRAT_40   0
#define LV_FONT_MONTSERRAT_42   0
#define LV_FONT_MONTSERRAT_44   0
#define LV_FONT_MONTSERRAT_46   0
#define LV_FONT_MONTSERRAT_48   1
#define LV_FONT_DEFAULT         &lv_font_montserrat_14

/*==================== TEXT ====================*/
#define LV_TXT_ENC              LV_TXT_ENC_UTF8

/*==================== WIDGETS ====================*/
#define LV_USE_ARC          1
#define LV_USE_BAR          1
#define LV_USE_BTN          1
#define LV_USE_BTNMATRIX    1
#define LV_USE_CANVAS       0
#define LV_USE_CHECKBOX     1
#define LV_USE_DROPDOWN     1
#define LV_USE_IMG          1
#define LV_USE_LABEL        1
#define LV_USE_LINE         1
#define LV_USE_ROLLER       1
#define LV_USE_SLIDER       1
#define LV_USE_SWITCH       1
#define LV_USE_TEXTAREA     1
#define LV_USE_TABLE        1

/*==================== EXTRA WIDGETS ====================*/
#define LV_USE_ANIMIMG      0
#define LV_USE_CALENDAR     0
#define LV_USE_CHART        0
#define LV_USE_COLORWHEEL   0
#define LV_USE_IMGBTN       1
#define LV_USE_KEYBOARD     1
#define LV_USE_LED          1
#define LV_USE_LIST         1
#define LV_USE_MENU         0
#define LV_USE_METER        0
#define LV_USE_MSGBOX       1
#define LV_USE_SPAN         0
#define LV_USE_SPINBOX      0
#define LV_USE_SPINNER      1
#define LV_USE_TABVIEW      1
#define LV_USE_TILEVIEW     0
#define LV_USE_WIN          1

/*==================== THEMES ====================*/
#define LV_USE_THEME_DEFAULT    1
#define LV_THEME_DEFAULT_DARK   1   /* 0=light, 1=dark */
#define LV_USE_THEME_SIMPLE     1
#define LV_USE_THEME_MONO       0

/*==================== DEMOS ====================*/
#define LV_USE_DEMO_WIDGETS     0
#define LV_USE_DEMO_BENCHMARK   0
#define LV_USE_DEMO_STRESS      0
#define LV_USE_DEMO_MUSIC       0

#endif /* LV_CONF_H */
#endif /* #if 1 */
