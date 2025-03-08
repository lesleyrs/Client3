#ifdef __3DS__
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include <3ds.h>
#include <malloc.h>

#include "../client.h"
#include "../gameshell.h"
#include "../pixmap.h"
#include "../platform.h"
#include "../inputtracking.h"

// NOTE: if screens are swapped on 3ds you have to change screen_width/height directly
// top screen
// #define SCREEN_WIDTH 400
// bottom screen
#undef SCREEN_WIDTH
#undef SCREEN_HEIGHT

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

extern ClientData _Client;
extern InputTracking _InputTracking;

static u32 *SOC_buffer = NULL;
#define SOC_ALIGN 0x1000
#define SOC_BUFFER_SIZE 0x100000

static uint8_t *fb_top = NULL;
static uint8_t *fb_bottom = NULL;

static void soc_shutdown() {
    socExit();

    // _3ds_toggle_top_screen(1);
}

void platform_init(void) {
    osSetSpeedupEnable(true);

    // gfxInit(GSP_BGR8_OES, GSP_BGR8_OES, 0);
    gfxInitDefault();
    /* uncomment and disable draw_top_background to see stdout */
    consoleInit(GFX_TOP, NULL);
}

void platform_new(GameShell *shell, int width, int height) {
    (void)shell, (void)width, (void)height;
    atexit(soc_shutdown);

    // NOTE we could use romfs, but we need sdcard to store the 3dsx anyway
    /* Result romfs_res = romfsInit();

    if (romfs_res) {
        rs2_error("romfsInit: %08lX\n", romfs_res);
        exit(1);
    } */

    gfxSetDoubleBuffering(GFX_BOTTOM, 0);
    gfxSetDoubleBuffering(GFX_TOP, 0);

    // mud->_3ds_framebuffer_top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);

    fb_bottom = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);

    /* allocate buffer for SOC service (networking) */
    SOC_buffer = (u32 *)memalign(SOC_ALIGN, SOC_BUFFER_SIZE);

    if (SOC_buffer == NULL) {
        rs2_error("memalign() fail\n");
        exit(1);
    }

    int ret = -1;

    if ((ret = socInit(SOC_buffer, SOC_BUFFER_SIZE)) != 0) {
        rs2_error("socInit: 0x%08X\n", (unsigned int)ret);
        exit(1);
    }

    /* audio_buffer =
        (u32 *)linearAlloc(SAMPLE_BUFFER_SIZE * BYTES_PER_SAMPLE * 2);

    ndspInit();

    ndspSetOutputMode(NDSP_OUTPUT_MONO);

    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, SAMPLE_RATE);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);

    float mix[12] = {0};
    mix[0] = 1.0;
    mix[1] = 1.0;

    ndspChnSetMix(0, mix);

    wave_buf[0].data_vaddr = &audio_buffer[0];
    wave_buf[0].nsamples = SAMPLE_BUFFER_SIZE;

    // wave_buf[1].data_vaddr = &audio_buffer[SAMPLE_BUFFER_SIZE];
    // wave_buf[1].nsamples = SAMPLE_BUFFER_SIZE;

    ndspChnWaveBufAdd(0, &wave_buf[0]);
    // ndspChnWaveBufAdd(0, &wave_buf[1]); */

    // HIDUSER_EnableGyroscope();
}
void platform_free(GameShell *shell) {
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
Surface *platform_create_surface(int *pixels, int width, int height, int alpha) {
    Surface *surface = calloc(1, sizeof(Surface));
    surface->pixels = pixels;
    return surface;
}
void platform_free_surface(Surface *surface) {
    free(surface);
}
int *get_pixels(Surface *surface) {
    return surface->pixels;
}
void set_pixels(PixMap *pixmap, int x, int y) {
    // TODO rm temp checks for going past framebuffer
    for (int row = 0; row < pixmap->height; row++) {
        if (y + row >= SCREEN_HEIGHT) break;

        for (int col = 0; col < pixmap->width; col++) {
            if (x + col >= SCREEN_WIDTH) break;

            int src_offset = row * pixmap->width + col;

            int pixel_offset = (x + col) * SCREEN_HEIGHT * 3 + (SCREEN_HEIGHT - 1 - (y + row)) * 3;
            // int pixel_offset = (y + row) * SCREEN_HEIGHT * 3 + (x + col) * 3;

            int pixel = pixmap->pixels[src_offset];
            uint8_t b = pixel & 0xff;
            uint8_t g = (pixel >> 8) & 0xff;
            uint8_t r = (pixel >> 16) & 0xff;

            fb_bottom[pixel_offset] = b;
            fb_bottom[pixel_offset + 1] = g;
            fb_bottom[pixel_offset + 2] = r;
        }
    }

    // gfxFlushBuffers();
    // gfxSwapBuffers();
    // gspWaitForVBlank();
}

void platform_get_keycodes(Keysym *keysym, int *code, char *ch) {
}

void platform_poll_events(Client *c) {
    hidScanInput();

    u32 keys_down = hidKeysDown();

    if (keys_down & KEY_LEFT) {
        key_pressed(c->shell, K_LEFT, -1);
    }

    if (keys_down & KEY_RIGHT) {
        key_pressed(c->shell, K_RIGHT, -1);
    }

    if (keys_down & KEY_UP) {
        key_pressed(c->shell, K_UP, -1);
    }

    if (keys_down & KEY_DOWN) {
        key_pressed(c->shell, K_DOWN, -1);
    }

    u32 keys_up = hidKeysUp();

    if (keys_up & KEY_LEFT) {
        key_released(c->shell, K_LEFT, -1);
    }

    if (keys_up & KEY_RIGHT) {
        key_released(c->shell, K_RIGHT, -1);
    }

    if (keys_up & KEY_UP) {
        key_released(c->shell, K_UP, -1);
    }

    if (keys_up & KEY_DOWN) {
        key_released(c->shell, K_DOWN, -1);
    }


    if (keys_down & KEY_X) {
        key_pressed(c->shell, K_CONTROL, -1);
    }

    if (keys_up & KEY_X) {
        key_released(c->shell, K_CONTROL, -1);
    }

    touchPosition touch = {0};
    hidTouchRead(&touch);

    static bool right_touch = false;
    if (keys_down & KEY_L) {
        right_touch = true;
    }

    if (keys_up & KEY_L) {
        right_touch = false;
    }

    static bool touch_down = false;
    if (touch.px == 0 && touch.py == 0) {
        if (touch_down) {
            c->shell->idle_cycles = 0;
            c->shell->mouse_button = 0;

            if (_InputTracking.enabled) {
                inputtracking_mouse_released(&_InputTracking, right_touch ? 1 : 0);
            }

            touch_down = false;
        }
    } else {
        int x = touch.px;
        int y = touch.py;

        // TODO: custom but has issue where it sometimes doesn't click or uses last pos
        c->shell->idle_cycles = 0;
        c->shell->mouse_x = x;
        c->shell->mouse_y = y;

        if (_InputTracking.enabled) {
            inputtracking_mouse_moved(&_InputTracking, x, y);
        }

        if (!touch_down) {
            // c->shell->idle_cycles = 0;
            c->shell->mouse_click_x = x;
            c->shell->mouse_click_y = y;

            if (right_touch) {
                c->shell->mouse_click_button = 2;
                c->shell->mouse_button = 2;
            } else {
                c->shell->mouse_click_button = 1;
                c->shell->mouse_button = 1;
            }

            if (_InputTracking.enabled) {
                inputtracking_mouse_pressed(&_InputTracking, x, y, right_touch ? 1 : 0);
            }

            touch_down = true;
        }
    }
}
void platform_update_surface(GameShell *shell) {
}
void platform_fill_rect(GameShell *shell, int x, int y, int w, int h, int color) {
}
void platform_blit_surface(GameShell *shell, int x, int y, int w, int h, Surface *surface) {
}
uint64_t get_ticks(void) {
    // return osGetTime();
    return (uint64_t)(svcGetSystemTick() / CPU_TICKS_PER_MSEC);
}
void delay_ticks(int ticks) {
    svcSleepThread((s64)ticks * 1000000);
    /* int end = get_ticks() + ticks;

    while (get_ticks() != end)
        ; */
}
#endif
