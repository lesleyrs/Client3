#ifdef __3DS__
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <3ds.h>
#include <malloc.h>

#include "../client.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../pixmap.h"
#include "../platform.h"

extern ClientData _Client;
extern InputTracking _InputTracking;

static u32 *SOC_buffer = NULL;
#define SOC_ALIGN 0x1000
#define SOC_BUFFER_SIZE 0x100000

static uint8_t *fb_top = NULL;
static uint8_t *fb_bottom = NULL;

#define SCREEN_FB_WIDTH_TOP 400
static int screen_offset_x_top = (SCREEN_FB_WIDTH_TOP - SCREEN_WIDTH) / 2;
static int screen_offset_y_top = 0;

static int screen_offset_x = (SCREEN_FB_WIDTH - SCREEN_WIDTH) / 2;
static int screen_offset_y = -SCREEN_FB_HEIGHT;

static void soc_shutdown() {
    socExit();

    // _3ds_toggle_top_screen(1);
}

bool platform_init(void) {
    osSetSpeedupEnable(true);

    gfxInit(GSP_RGBA8_OES, GSP_RGBA8_OES, 0);
    // gfxInitDefault();
    /* uncomment and disable draw_top_background to see stdout */
    // consoleInit(GFX_TOP, NULL);
    return true;
}

void platform_new(int width, int height) {
    (void)width, (void)height;
    atexit(soc_shutdown);

    // NOTE we could use romfs, but we need sdcard to store the 3dsx anyway?
    /* Result romfs_res = romfsInit();

    if (romfs_res) {
        rs2_error("romfsInit: %08lX\n", romfs_res);
        exit(1);
    } */

    gfxSetDoubleBuffering(GFX_BOTTOM, 0);
    gfxSetDoubleBuffering(GFX_TOP, 0);

    fb_top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
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
void platform_free(void) {
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
void set_pixels(PixMap *pixmap, int x, int y) {
    uint32_t *fb32_top = (uint32_t *)fb_top;
    uint32_t *fb32_bottom = (uint32_t *)fb_bottom;

    for (int row = 0; row < pixmap->height; row++) {
        int screen_y = y + row + screen_offset_y;
        if (screen_y < 0)
            continue;
        if (screen_y >= SCREEN_FB_HEIGHT)
            break;

        for (int col = 0; col < pixmap->width; col++) {
            int screen_x = x + col + screen_offset_x;
            if (screen_x < 0)
                continue;
            if (screen_x >= SCREEN_FB_WIDTH)
                break;

            int pixel = pixmap->pixels[row * pixmap->width + col];
            int dst = screen_x * SCREEN_FB_HEIGHT + (SCREEN_FB_HEIGHT - 1 - screen_y);

            fb32_bottom[dst] = pixel << 8;
        }
    }

    if (fb_top) {
        for (int row = 0; row < pixmap->height; row++) {
            int screen_y = y + row + screen_offset_y_top;
            if (screen_y < 0)
                continue;
            if (screen_y >= SCREEN_FB_HEIGHT)
                break;

            for (int col = 0; col < pixmap->width; col++) {
                int screen_x = x + col + screen_offset_x_top;
                if (screen_x < 0)
                    continue;
                if (screen_x >= SCREEN_FB_WIDTH_TOP)
                    break;

                int pixel = pixmap->pixels[row * pixmap->width + col];
                int dst = screen_x * SCREEN_FB_HEIGHT + (SCREEN_FB_HEIGHT - 1 - screen_y);

                fb32_top[dst] = pixel << 8;
            }
        }
    }
    // gfxFlushBuffers();
    // gfxSwapBuffers();
    // gspWaitForVBlank();
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
        int x = touch.px - screen_offset_x;
        int y = touch.py - screen_offset_y;

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
void platform_update_surface(void) {
}
void platform_fill_rect(int x, int y, int w, int h, int color) {
}
void platform_blit_surface(int x, int y, int w, int h, Surface *surface) {
}
uint64_t get_ticks(void) {
    // return osGetTime();
    return (uint64_t)(svcGetSystemTick() / CPU_TICKS_PER_MSEC);
}
void delay_ticks(int ticks) {
    svcSleepThread(ticks * 1000000);
}
#endif
