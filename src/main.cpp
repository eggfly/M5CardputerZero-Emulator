#include "lvgl/lvgl.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

extern "C" { void ui_init(void); }

// ── Layout detected from M5CardputerEmu.png (1280x840 RGBA) ────
static constexpr int SKIN_W = 1280;
static constexpr int SKIN_H = 840;

// LCD transparent area in skin
static constexpr int LCD_SX = 325;
static constexpr int LCD_SY = 60;
static constexpr int LCD_SW = 639;
static constexpr int LCD_SH = 339;
static constexpr int LCD_W  = 320;
static constexpr int LCD_H  = 170;

// Key hit rectangles in skin pixel coords
struct KeyRect { int x, y, w, h; SDL_Keycode key; };
static KeyRect g_keys[4][11] = {
    // Row 0: numbers
    {{51,461,70,41,SDLK_1},{162,461,71,42,SDLK_2},{274,461,71,41,SDLK_3},{386,461,71,41,SDLK_4},
     {497,461,71,41,SDLK_5},{610,461,69,41,SDLK_6},{720,461,71,42,SDLK_7},{832,461,71,42,SDLK_8},
     {944,461,70,42,SDLK_9},{1056,461,71,41,SDLK_0},{1168,461,70,42,SDLK_BACKSPACE}},
    // Row 1: QWERTY
    {{51,558,71,42,SDLK_TAB},{162,558,71,42,SDLK_q},{274,558,71,42,SDLK_w},{386,558,70,42,SDLK_e},
     {497,558,71,42,SDLK_r},{610,558,69,42,SDLK_t},{720,558,71,42,SDLK_y},{832,558,71,42,SDLK_u},
     {944,558,70,42,SDLK_i},{1056,558,71,42,SDLK_o},{1168,558,70,42,SDLK_p}},
    // Row 2: ASDF
    {{51,655,70,41,SDLK_LSHIFT},{162,655,71,41,SDLK_a},{274,655,71,41,SDLK_s},{386,655,70,41,SDLK_d},
     {497,655,71,41,SDLK_f},{610,655,69,41,SDLK_g},{720,655,71,41,SDLK_h},{832,655,71,41,SDLK_j},
     {944,655,70,41,SDLK_k},{1056,655,71,42,SDLK_l},{1168,655,70,41,SDLK_RETURN}},
    // Row 3: ZXCV
    {{51,752,70,41,SDLK_ESCAPE},{162,752,71,41,SDLK_LCTRL},{274,752,71,41,SDLK_LALT},
     {386,752,70,41,SDLK_z},{497,752,71,41,SDLK_x},{610,752,69,41,SDLK_c},
     {720,752,71,41,SDLK_v},{832,752,71,41,SDLK_b},{944,752,70,41,SDLK_n},
     {1056,752,71,41,SDLK_m},{1168,752,70,41,SDLK_SPACE}},
};

static int g_pr = -1, g_pc = -1;

static bool hit_key(int mx, int my, int *r, int *c)
{
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 11; j++) {
            auto &k = g_keys[i][j];
            if (mx >= k.x && mx < k.x+k.w && my >= k.y && my < k.y+k.h) {
                *r = i; *c = j; return true;
            }
        }
    return false;
}

int main(int, char *[])
{
    printf("========================================\n");
    printf("  M5CardputerZero Emulator\n");
    printf("  Skin: %dx%d  LCD: %dx%d at (%d,%d)\n",
           SKIN_W, SKIN_H, LCD_W, LCD_H, LCD_SX, LCD_SY);
    printf("========================================\n");

    lv_init();

    // LVGL 320x170 display
    lv_display_t *disp = lv_sdl_window_create(LCD_W, LCD_H);
    if (!disp) { fprintf(stderr, "display failed\n"); return 1; }

    // Get LVGL's SDL window + renderer, resize to skin size
    SDL_Window *win = lv_sdl_window_get_window(disp);
    SDL_Renderer *ren = (SDL_Renderer *)lv_sdl_window_get_renderer(disp);
    static constexpr float SCALE = 0.5f;
    int win_w = (int)(SKIN_W * SCALE);
    int win_h = (int)(SKIN_H * SCALE);
    SDL_SetWindowSize(win, win_w, win_h);
    SDL_SetWindowTitle(win, "M5CardputerZero Emulator");
    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    // Load skin overlay (RGBA with transparent LCD hole)
    IMG_Init(IMG_INIT_PNG);
    SDL_Surface *surf = IMG_Load("assets/device_skin.png");
    if (!surf) { fprintf(stderr, "skin load failed: %s\n", IMG_GetError()); return 1; }
    SDL_Texture *skin = SDL_CreateTextureFromSurface(ren, surf);
    SDL_SetTextureBlendMode(skin, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(surf);

    // LVGL input
    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();

    // APPLaunch
    ui_init();

    // Set LVGL rendering viewport to LCD area within the big window
    SDL_Rect lcd_vp = {(int)(LCD_SX*SCALE), (int)(LCD_SY*SCALE), (int)(LCD_SW*SCALE), (int)(LCD_SH*SCALE)};

    printf("[EMU] Running.\n");
    while (true) {
        // Handle virtual key clicks
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) goto done;

            if (ev.type == SDL_MOUSEBUTTONDOWN) {
                int r, c;
                int sx = (int)(ev.button.x / SCALE);
                int sy = (int)(ev.button.y / SCALE);
                if (hit_key(sx, sy, &r, &c)) {
                    g_pr = r; g_pc = c;
                    SDL_Event ke = {};
                    ke.type = SDL_KEYDOWN;
                    ke.key.windowID = SDL_GetWindowID(win);
                    ke.key.keysym.sym = g_keys[r][c].key;
                    ke.key.keysym.scancode = SDL_GetScancodeFromKey(g_keys[r][c].key);
                    ke.key.state = SDL_PRESSED;
                    SDL_PushEvent(&ke);
                    SDL_Keycode k = g_keys[r][c].key;
                    if (k >= 0x20 && k < 0x7f) {
                        SDL_Event te = {};
                        te.type = SDL_TEXTINPUT;
                        te.text.windowID = SDL_GetWindowID(win);
                        te.text.text[0] = (char)k;
                        te.text.text[1] = '\0';
                        SDL_PushEvent(&te);
                    }
                } else {
                    // Click in LCD area? Convert to LVGL coords
                    // (LVGL's mouse handler will be confused by the offset)
                    // Push the event back for LVGL to handle
                    SDL_PushEvent(&ev);
                }
            } else if (ev.type == SDL_MOUSEBUTTONUP) {
                if (g_pr >= 0) {
                    SDL_Event ke = {};
                    ke.type = SDL_KEYUP;
                    ke.key.windowID = SDL_GetWindowID(win);
                    ke.key.keysym.sym = g_keys[g_pr][g_pc].key;
                    ke.key.state = SDL_RELEASED;
                    SDL_PushEvent(&ke);
                    g_pr = -1;
                } else {
                    SDL_PushEvent(&ev);
                }
            } else {
                // Forward keyboard events etc to LVGL
                SDL_PushEvent(&ev);
            }
        }

        // Render: set viewport to LCD area, let LVGL render there
        SDL_RenderSetViewport(ren, &lcd_vp);
        lv_timer_handler();

        // Reset viewport and draw skin overlay on top
        SDL_RenderSetViewport(ren, nullptr);
        SDL_RenderCopy(ren, skin, nullptr, nullptr);

        // Highlight pressed key
        if (g_pr >= 0) {
            auto &k = g_keys[g_pr][g_pc];
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 255, 50, 50, 100);
            SDL_Rect kr = {(int)(k.x*SCALE), (int)(k.y*SCALE), (int)(k.w*SCALE), (int)(k.h*SCALE)};
            SDL_RenderFillRect(ren, &kr);
        }

        SDL_RenderPresent(ren);
        usleep(5000);
    }

done:
    SDL_DestroyTexture(skin);
    IMG_Quit();
    return 0;
}
