#include "lvgl/lvgl.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <dlfcn.h>

// ── Layout from M5CardputerEmu.png (1280x840 RGBA) ─────────────
static constexpr int SKIN_W = 1280;
static constexpr int SKIN_H = 840;
static constexpr int LCD_SX = 325;
static constexpr int LCD_SY = 60;
static constexpr int LCD_SW = 639;
static constexpr int LCD_SH = 339;
static constexpr int LCD_W  = 320;
static constexpr int LCD_H  = 170;
static constexpr float SCALE = 0.5f;

struct KeyRect { int x, y, w, h; SDL_Keycode key; };
static KeyRect g_keys[4][11] = {
    {{51,461,70,41,SDLK_1},{162,461,71,42,SDLK_2},{274,461,71,41,SDLK_3},{386,461,71,41,SDLK_4},
     {497,461,71,41,SDLK_5},{610,461,69,41,SDLK_6},{720,461,71,42,SDLK_7},{832,461,71,42,SDLK_8},
     {944,461,70,42,SDLK_9},{1056,461,71,41,SDLK_0},{1168,461,70,42,SDLK_BACKSPACE}},
    {{51,558,71,42,SDLK_TAB},{162,558,71,42,SDLK_q},{274,558,71,42,SDLK_w},{386,558,70,42,SDLK_e},
     {497,558,71,42,SDLK_r},{610,558,69,42,SDLK_t},{720,558,71,42,SDLK_y},{832,558,71,42,SDLK_u},
     {944,558,70,42,SDLK_i},{1056,558,71,42,SDLK_o},{1168,558,70,42,SDLK_p}},
    {{51,655,70,41,SDLK_LSHIFT},{162,655,71,41,SDLK_a},{274,655,71,41,SDLK_s},{386,655,70,41,SDLK_d},
     {497,655,71,41,SDLK_f},{610,655,69,41,SDLK_g},{720,655,71,41,SDLK_h},{832,655,71,41,SDLK_j},
     {944,655,70,41,SDLK_k},{1056,655,71,42,SDLK_l},{1168,655,70,41,SDLK_RETURN}},
    {{51,752,70,41,SDLK_ESCAPE},{162,752,71,41,SDLK_LCTRL},{274,752,71,41,SDLK_LALT},
     {386,752,70,41,SDLK_z},{497,752,71,41,SDLK_x},{610,752,69,41,SDLK_c},
     {720,752,71,41,SDLK_v},{832,752,71,41,SDLK_b},{944,752,70,41,SDLK_n},
     {1056,752,71,41,SDLK_m},{1168,752,70,41,SDLK_SPACE}},
};

static int g_pr = -1, g_pc = -1;
static SDL_Window   *g_win = nullptr;
static SDL_Renderer *g_ren = nullptr;
static SDL_Texture  *g_skin_tex = nullptr;
static SDL_Texture  *g_lcd_tex = nullptr;
static uint32_t     *g_lcd_buf = nullptr;

// Function pointers loaded from app dylib
typedef void (*sdl_kbd_handler_fn)(SDL_Event *);
typedef void *(*sdl_kbd_create_fn)(void);
static sdl_kbd_handler_fn g_kbd_handler = nullptr;

static bool hit_key(int mx, int my, int *r, int *c)
{
    int sx = (int)(mx / SCALE), sy = (int)(my / SCALE);
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 11; j++) {
            auto &k = g_keys[i][j];
            if (sx >= k.x && sx < k.x+k.w && sy >= k.y && sy < k.y+k.h)
                { *r = i; *c = j; return true; }
        }
    return false;
}

// ── LVGL flush: RGB565 → ARGB8888 ──────────────────────────────
static void flush_cb(lv_display_t *, const lv_area_t *area, uint8_t *px)
{
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    uint16_t *src = (uint16_t *)px;
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            uint16_t c = src[y * w + x];
            uint8_t r5 = (c >> 11) & 0x1F;
            uint8_t g6 = (c >> 5) & 0x3F;
            uint8_t b5 = c & 0x1F;
            g_lcd_buf[(area->y1+y)*LCD_W + area->x1+x] = 0xFF000000
                | ((r5<<3|r5>>2)<<16) | ((g6<<2|g6>>4)<<8) | (b5<<3|b5>>2);
        }
    }
    lv_display_flush_ready(lv_display_get_default());
}

// ── Inject SDL key event (for virtual keyboard clicks) ──────────
static void inject_sdl_key(SDL_Keycode key, bool down)
{
    SDL_Event ev = {};
    ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    ev.key.windowID = SDL_GetWindowID(g_win);
    ev.key.keysym.sym = key;
    ev.key.keysym.scancode = SDL_GetScancodeFromKey(key);
    ev.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    if (g_kbd_handler) g_kbd_handler(&ev);

    // Also send TEXTINPUT for printable chars
    if (down && key >= 0x20 && key < 0x7f && g_kbd_handler) {
        SDL_Event te = {};
        te.type = SDL_TEXTINPUT;
        te.text.windowID = SDL_GetWindowID(g_win);
        te.text.text[0] = (char)key;
        te.text.text[1] = '\0';
        g_kbd_handler(&te);
    }
}

static void render()
{
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
    SDL_RenderClear(g_ren);

    // Skin background
    SDL_RenderCopy(g_ren, g_skin_tex, nullptr, nullptr);

    // LCD content
    SDL_UpdateTexture(g_lcd_tex, nullptr, g_lcd_buf, LCD_W * 4);
    SDL_Rect lcd_dst = {(int)(LCD_SX*SCALE),(int)(LCD_SY*SCALE),
                        (int)(LCD_SW*SCALE),(int)(LCD_SH*SCALE)};
    SDL_RenderCopy(g_ren, g_lcd_tex, nullptr, &lcd_dst);

    // Skin overlay (transparent LCD hole lets content show)
    SDL_RenderCopy(g_ren, g_skin_tex, nullptr, nullptr);

    // Key press highlight
    if (g_pr >= 0) {
        auto &k = g_keys[g_pr][g_pc];
        SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_ren, 255, 50, 50, 100);
        SDL_Rect kr = {(int)(k.x*SCALE),(int)(k.y*SCALE),
                       (int)(k.w*SCALE),(int)(k.h*SCALE)};
        SDL_RenderFillRect(g_ren, &kr);
    }

    SDL_RenderPresent(g_ren);
}

typedef void (*ui_init_fn)(void);

int main(int argc, char *argv[])
{
    const char *app_path = "apps/libAPPLaunch.dylib";
    if (argc > 1) app_path = argv[1];

    printf("========================================\n");
    printf("  M5CardputerZero Emulator\n");
    printf("  App : %s\n", app_path);
    printf("  LCD : %dx%d  Window: %dx%d\n",
           LCD_W, LCD_H, (int)(SKIN_W*SCALE), (int)(SKIN_H*SCALE));
    printf("========================================\n");

    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    int win_w = (int)(SKIN_W * SCALE), win_h = (int)(SKIN_H * SCALE);
    g_win = SDL_CreateWindow("M5CardputerZero Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h, SDL_WINDOW_SHOWN);
    g_ren = SDL_CreateRenderer(g_win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    // Skin texture
    SDL_Surface *surf = IMG_Load("assets/device_skin.png");
    if (!surf) { fprintf(stderr, "skin: %s\n", IMG_GetError()); return 1; }
    g_skin_tex = SDL_CreateTextureFromSurface(g_ren, surf);
    SDL_SetTextureBlendMode(g_skin_tex, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(surf);

    // LCD texture
    g_lcd_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, LCD_W, LCD_H);
    g_lcd_buf = (uint32_t *)calloc(LCD_W * LCD_H, sizeof(uint32_t));

    // LVGL init
    lv_init();
    static uint8_t draw_buf[LCD_W * LCD_H * 2];
    lv_display_t *disp = lv_display_create(LCD_W, LCD_H);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_buffers(disp, draw_buf, nullptr, sizeof(draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);

    // Load app
    void *app = dlopen(app_path, RTLD_NOW | RTLD_GLOBAL);
    if (!app) { fprintf(stderr, "[EMU] dlopen: %s\n", dlerror()); return 1; }
    printf("[EMU] Loaded: %s\n", app_path);

    // Try to find app's keyboard handler (APPLaunch overrides lv_sdl_keyboard_*)
    auto kbd_create = (sdl_kbd_create_fn)dlsym(app, "lv_sdl_keyboard_create");
    g_kbd_handler = (sdl_kbd_handler_fn)dlsym(app, "lv_sdl_keyboard_handler");

    printf("[EMU] kbd_create=%p  kbd_handler=%p\n", (void*)kbd_create, (void*)g_kbd_handler);

    if (kbd_create) {
        printf("[EMU] Using app keyboard driver\n");
        void *indev = kbd_create();
        printf("[EMU] kbd_create returned indev=%p\n", indev);
    } else {
        printf("[EMU] Using built-in keyboard driver\n");
        lv_indev_t *kb = lv_indev_create();
        lv_indev_set_type(kb, LV_INDEV_TYPE_KEYPAD);
    }

    // List all indevs
    {
        lv_indev_t *id = lv_indev_get_next(NULL);
        int n = 0;
        while (id) { n++; printf("[EMU] indev #%d: type=%d\n", n, lv_indev_get_type(id)); id = lv_indev_get_next(id); }
        printf("[EMU] Total indevs: %d\n", n);
    }

    // Init app
    auto init = (ui_init_fn)dlsym(app, "ui_init");
    if (!init) { fprintf(stderr, "[EMU] ui_init missing\n"); return 1; }
    init();
    printf("[EMU] Running. Keyboard active.\n");

    while (true) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) goto done;

            // Virtual keyboard clicks
            if (ev.type == SDL_MOUSEBUTTONDOWN) {
                int r, c;
                if (hit_key(ev.button.x, ev.button.y, &r, &c)) {
                    g_pr = r; g_pc = c;
                    printf("[EMU] vkey click row=%d col=%d sdlk=%d\n", r, c, g_keys[r][c].key);
                    inject_sdl_key(g_keys[r][c].key, true);
                }
            }
            else if (ev.type == SDL_MOUSEBUTTONUP && g_pr >= 0) {
                inject_sdl_key(g_keys[g_pr][g_pc].key, false);
                g_pr = -1;
            }
            // PC keyboard → forward to app's SDL keyboard handler
            else if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP ||
                     ev.type == SDL_TEXTINPUT) {
                if (ev.type == SDL_KEYDOWN)
                    printf("[EMU] SDL_KEYDOWN sym=%d scan=%d winID=%d\n", ev.key.keysym.sym, ev.key.keysym.scancode, ev.key.windowID);
                if (ev.type == SDL_TEXTINPUT)
                    printf("[EMU] SDL_TEXTINPUT text='%s' winID=%d\n", ev.text.text, ev.text.windowID);
                if (g_kbd_handler) {
                    g_kbd_handler(&ev);
                } else {
                    printf("[EMU] NO kbd_handler!\n");
                }
            }
        }

        lv_tick_inc(5);
        lv_timer_handler();
        render();
        SDL_Delay(5);
    }

done:
    free(g_lcd_buf);
    dlclose(app);
    SDL_DestroyTexture(g_lcd_tex);
    SDL_DestroyTexture(g_skin_tex);
    SDL_DestroyRenderer(g_ren);
    SDL_DestroyWindow(g_win);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
