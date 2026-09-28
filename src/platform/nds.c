#ifdef __NDS__
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dswifi9.h>
#include <filesystem.h>
#include <nds.h>
#include <wfc.h>

#include "../client.h"
#include "../gameshell.h"
#include "../pixmap.h"
#include "../platform.h"
#include "../defines.h"
#include "../inputtracking.h"

extern InputTracking _InputTracking;

// static int screen_offset_x = (SCREEN_FB_WIDTH - SCREEN_WIDTH) / 2;
// static int screen_offset_y = -200;
// static int screen_offset_x = -8;
// static int screen_offset_y = -11;
static int screen_offset_x = 0;
static int screen_offset_y = 0;

static touchPosition touch;

static uint16_t *fb = (uint16_t *)VRAM_A;

bool platform_init(void) {
    cpuStartTiming(0xdeadbeef); // NOTE unused value, but not in blocksds?
    lcdMainOnBottom();
    consoleDemoInit();
    // consoleDebugInit(DebugDevice_NOCASH); // melonDS spams too many networking logs
    videoSetMode(MODE_FB0);
    vramSetBankA(VRAM_A_LCD);

    if (!isDSiMode()) {
        rs2_error("NDS detected! only DSi is supported.\n");
        return false;
    }
    if (!nitroFSInit(NULL)) {
        rs2_error("nitroFS init failed\n");
        return false;
    }
    chdir("nitro:/");

    // TODO move to clientstream_init? and maybe allow retries?
    if (!Wifi_InitDefault(WFC_CONNECT)) {
        rs2_error("Failed to connect!\n");
        return false;
    }

    return true;
}
void platform_new(GameShell *shell) {
    (void)shell;
}
void platform_free(void) {
    Wifi_DisconnectAP();
}
void platform_set_wave_volume(int wavevol) {
}
void platform_play_wave(int8_t *src, int length) {
}
void platform_set_midi_volume(float midivol) {
}
void platform_set_jingle(int8_t *src, int len) {
}
void platform_set_midi(const char *name, int crc, int len) {
}
void platform_stop_midi(void) {
}
void platform_poll_events(Client *c) {
    touchPosition last = touch;
    touchRead(&touch);

    scanKeys();
    int pressed = keysDown();
    int released = keysUp();

    // TODO right press/release
    if (pressed & KEY_TOUCH) {
        int x = touch.px - screen_offset_x;
        int y = touch.py - screen_offset_y;

        // moved
        if (touch.px != last.px || touch.py != last.py) {
            c->shell->idle_cycles = 0;
            c->shell->mouse_x = x;
            c->shell->mouse_y = y;

            if (_InputTracking.enabled) {
                inputtracking_mouse_moved(&_InputTracking, x, y);
            }
        }

        c->shell->mouse_click_x = x;
        c->shell->mouse_click_y = y;

        // if (e.button.button == SDL_BUTTON_RIGHT) {
        //     c->shell->mouse_click_button = 2;
        //     c->shell->mouse_button = 2;
        // } else {
            c->shell->mouse_click_button = 1;
            c->shell->mouse_button = 1;
        // }

        if (_InputTracking.enabled) {
            // inputtracking_mouse_pressed(&_InputTracking, x, y, e.button.button == SDL_BUTTON_RIGHT ? 1 : 0);
        }
    }

    if (released & KEY_TOUCH) {
        c->shell->idle_cycles = 0;
        c->shell->mouse_button = 0;

        if (_InputTracking.enabled) {
            // inputtracking_mouse_released(&_InputTracking, (e.button.button & SDL_BUTTON_RMASK) != 0 ? 1 : 0);
        }
    }

    if (pressed & KEY_UP) {
        key_pressed(c->shell, K_UP, -1);
    }
    if (pressed & KEY_DOWN) {
        key_pressed(c->shell, K_DOWN, -1);
    }
    if (pressed & KEY_LEFT) {
        key_pressed(c->shell, K_LEFT, -1);
    }
    if (pressed & KEY_RIGHT) {
        key_pressed(c->shell, K_RIGHT, -1);
    }

    if (released & KEY_UP) {
        key_released(c->shell, K_UP, -1);
    }
    if (released & KEY_DOWN) {
        key_released(c->shell, K_DOWN, -1);
    }
    if (released & KEY_LEFT) {
        key_released(c->shell, K_LEFT, -1);
    }
    if (released & KEY_RIGHT) {
        key_released(c->shell, K_RIGHT, -1);
    }
}
void platform_blit_surface(Surface *surface, int x, int y) {
    for (int row = 0; row < surface->h; row++) {
        int screen_y = y + row + screen_offset_y;
        if (screen_y < 0)
            continue;
        if (screen_y >= SCREEN_FB_HEIGHT)
            break;

        for (int col = 0; col < surface->w; col++) {
            int screen_x = x + col + screen_offset_x;
            if (screen_x < 0)
                continue;
            if (screen_x >= SCREEN_FB_WIDTH)
                break;

            int pixel = surface->pixels[row * surface->w + col];
            uint8_t r = (pixel >> 16) & 0xff;
            uint8_t g = (pixel >> 8) & 0xff;
            uint8_t b = pixel & 0xff;
            fb[screen_y * SCREEN_FB_WIDTH + screen_x] = RGB8(r, g, b);
        }
    }
}
void platform_update_surface(void) {
}
uint64_t rs2_now(void) {
    return timerTicks2msec(cpuGetTiming());
}
void rs2_sleep(int ms) {
    uint64_t end = rs2_now() + ms;
    while (rs2_now() != end)
        ;
}
#endif
