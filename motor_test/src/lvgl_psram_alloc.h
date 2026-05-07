/**
 * LVGL custom allocator - routes LVGL internal heap to 8 MB PSRAM.
 * Included via LV_MEM_CUSTOM_INCLUDE in lv_conf.h.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include "esp_heap_caps.h"

static inline void *lv_psram_malloc(size_t size) {
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static inline void *lv_psram_realloc(void *ptr, size_t size) {
    return heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

#ifdef __cplusplus
}
#endif
