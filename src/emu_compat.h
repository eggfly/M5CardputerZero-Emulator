#pragma once
#ifndef LV_COLOR_16_SWAP
#define LV_COLOR_16_SWAP 0
#endif

// Stub freetype API when disabled (Emscripten build)
#if defined(LV_USE_FREETYPE) && LV_USE_FREETYPE == 0
#ifndef LV_FREETYPE_FONT_RENDER_MODE_BITMAP
#define LV_FREETYPE_FONT_RENDER_MODE_BITMAP 0
#define LV_FREETYPE_FONT_STYLE_NORMAL 0
#define LV_FREETYPE_FONT_STYLE_BOLD   1
#include "lvgl/lvgl.h"
static inline lv_font_t *lv_freetype_font_create(const char *path, int mode, int size, int style)
{ (void)path; (void)mode; (void)size; (void)style; return NULL; }
#endif
#endif
