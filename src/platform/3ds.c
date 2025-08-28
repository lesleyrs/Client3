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
#include "../custom.h"

extern ClientData _Client;
extern InputTracking _InputTracking;
extern Custom _Custom;

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

// all arbitrary
#define CURSOR_SENSITIVITY 20
#define PAN_THRESHOLD 40
#define MAX_CIRCLEPAD_POS 150 // real hw seems to report up to 168

#define VIEWPORT_X_LEN 512
#define VIEWPORT_Y_LEN 334
#define VIEWPORT_X_OFF 8
#define VIEWPORT_Y_OFF 11

static void set_ingame_screen_pan(Client *c) {
    screen_offset_x_top = (-VIEWPORT_X_LEN + SCREEN_FB_WIDTH_TOP) / 2 - VIEWPORT_X_OFF;
    screen_offset_y_top = (-VIEWPORT_Y_LEN + SCREEN_FB_HEIGHT) / 2 - VIEWPORT_Y_OFF;

    screen_offset_x = (-VIEWPORT_X_LEN + SCREEN_FB_WIDTH) / 2 - VIEWPORT_X_OFF;
    screen_offset_y = (-VIEWPORT_Y_LEN + SCREEN_FB_HEIGHT) / 2 - VIEWPORT_Y_OFF;
    c->redraw_background = true;
}

static void set_title_screen_pan(Client *c) {
    screen_offset_x_top = (SCREEN_FB_WIDTH_TOP - SCREEN_WIDTH) / 2;
    screen_offset_y_top = 0;

    screen_offset_x = (SCREEN_FB_WIDTH - SCREEN_WIDTH) / 2;
    screen_offset_y = -SCREEN_FB_HEIGHT;
    c->redraw_background = true;
}

static void clamp_top_screen(int xoff, int yoff) {
    screen_offset_x_top = MAX(SCREEN_FB_WIDTH_TOP - SCREEN_WIDTH, MIN(screen_offset_x_top - xoff / CURSOR_SENSITIVITY, 0));
    screen_offset_y_top = MAX(SCREEN_FB_HEIGHT - SCREEN_HEIGHT, MIN(screen_offset_y_top + yoff / CURSOR_SENSITIVITY, 0));
}

static void clamp_bottom_screen(int xoff, int yoff) {
    screen_offset_x = MAX(SCREEN_FB_WIDTH - SCREEN_WIDTH, MIN(screen_offset_x - xoff / CURSOR_SENSITIVITY, 0));
    screen_offset_y = MAX(SCREEN_FB_HEIGHT - SCREEN_HEIGHT, MIN(screen_offset_y + yoff / CURSOR_SENSITIVITY, 0));
}

static void soc_shutdown(void) {
    socExit();
}

bool platform_init(void) {
    osSetSpeedupEnable(true);

    // gfxInitDefault();
    gfxInit(GSP_RGBA8_OES, GSP_RGBA8_OES, 0);
    /* NOTE: uncomment consoleInit and comment out fb_top to see stdout */
    // consoleInit(GFX_TOP, NULL);
    return true;
}

void platform_new(GameShell *shell) {
    (void)shell;
    atexit(soc_shutdown);

    // NOTE could use romfs, but we want access to config
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

void platform_update_touch(Client *c) {
    if (update_touch) {
        c->shell->mouse_click_x = tmp_mouse_click_x;
        c->shell->mouse_click_y = tmp_mouse_click_y;
        c->shell->mouse_click_button = tmp_mouse_click_button;
        c->shell->mouse_button = tmp_mouse_button;
        update_touch = false;
    }
}

void platform_poll_events(Client *c) {
    static bool ingame = false;
    if (ingame && !c->ingame) {
        set_title_screen_pan(c);
    } else if (!ingame && c->ingame) {
        set_ingame_screen_pan(c);
    }
    ingame = c->ingame;

    hidScanInput();

    u32 keys_down = hidKeysDown();
    u32 keys_up = hidKeysUp();
    u32 keys_held = hidKeysHeld();

    touchPosition touch = {0};
    hidTouchRead(&touch);

    circlePosition pos = {0};
    hidCircleRead(&pos);

    bool L_down = keys_held & KEY_L;
    bool R_down = keys_held & KEY_R;
    bool A_down = keys_held & KEY_A;

    if (L_down || R_down) {
        if (pos.dx < -PAN_THRESHOLD || pos.dx > PAN_THRESHOLD || pos.dy < -PAN_THRESHOLD || pos.dy > PAN_THRESHOLD) {
            if (L_down) {
                clamp_top_screen(pos.dx, pos.dy);
            } else {
                clamp_bottom_screen(pos.dx, pos.dy);
            }
            c->redraw_background = true;
        } else {
            if (keys_held & (KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN)) {
                int x = 0, y = 0;
                if (keys_held & KEY_LEFT) {
                    x -= MAX_CIRCLEPAD_POS;
                }

                if (keys_held & KEY_RIGHT) {
                    x += MAX_CIRCLEPAD_POS;
                }

                if (keys_held & KEY_UP) {
                    y += MAX_CIRCLEPAD_POS;
                }

                if (keys_held & KEY_DOWN) {
                    y -= MAX_CIRCLEPAD_POS;
                }
                if (L_down) {
                    clamp_top_screen(x, y);
                } else {
                    clamp_bottom_screen(x, y);
                }
                c->redraw_background = true;
            }
        }
    } else {
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
    }


    if (keys_down & KEY_B) {
        key_pressed(c->shell, K_CONTROL, -1);
    }

    if (keys_down & KEY_SELECT) {
        _Custom.showPerformance = !_Custom.showPerformance;
    }

    if (keys_down & KEY_START) {
        if (c->ingame) {
            set_ingame_screen_pan(c);
        } else {
            set_title_screen_pan(c);
        }
    }

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

    if (keys_up & KEY_B) {
        key_released(c->shell, K_CONTROL, -1);
    }

    static bool touch_down = false;
    if (touch.px == 0 && touch.py == 0) {
        if (touch_down) {
            c->shell->idle_cycles = 0;
            c->shell->mouse_button = 0;

            if (_InputTracking.enabled) {
                inputtracking_mouse_released(&_InputTracking, A_down ? 1 : 0);
            }

            touch_down = false;
        }
    } else {
        int x = touch.px - screen_offset_x;
        int y = touch.py - screen_offset_y;

        c->shell->idle_cycles = 0;
        c->shell->mouse_x = x;
        c->shell->mouse_y = y;

        if (_InputTracking.enabled) {
            inputtracking_mouse_moved(&_InputTracking, x, y);
        }

        if (!touch_down) {
            // c->shell->idle_cycles = 0;
            update_touch = true;

            tmp_mouse_click_x = x;
            tmp_mouse_click_y = y;

            if (A_down) {
                tmp_mouse_click_button = 2;
                tmp_mouse_button = 2;
            } else {
                tmp_mouse_click_button = 1;
                tmp_mouse_button = 1;
            }

            if (_InputTracking.enabled) {
                inputtracking_mouse_pressed(&_InputTracking, x, y, A_down ? 1 : 0);
            }

            touch_down = true;
        }
    }
}
void platform_blit_surface(int x, int y, int w, int h, Surface *surface) {
}
void platform_update_surface(void) {
}
void platform_draw_rect(int x, int y, int w, int h, int color) {
}
void platform_fill_rect(int x, int y, int w, int h, int color) {
}
uint64_t rs2_now(void) {
    // return osGetTime();
    return (uint64_t)(svcGetSystemTick() / CPU_TICKS_PER_MSEC);
}
void rs2_sleep(int ms) {
    svcSleepThread(ms * 1000000);
}
#endif
