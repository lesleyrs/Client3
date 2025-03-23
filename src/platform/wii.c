#ifdef __WII__
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <asndlib.h>
#include <fat.h>
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/usbmouse.h>
#include <ogcsys.h>
#include <wiikeyboard/keyboard.h>
#include <wiiuse/wpad.h>

#include "../client.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../pix2d.h"
#include "../pixmap.h"
#include "../platform.h"

extern ClientData _Client;
extern InputTracking _InputTracking;

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;
static int cursor_x = SCREEN_FB_WIDTH / 2;
static int cursor_y = SCREEN_FB_HEIGHT / 2;
#define INITIAL_SCREEN_X (SCREEN_FB_WIDTH - SCREEN_WIDTH) / 2
#define INITIAL_SCREEN_Y (SCREEN_FB_HEIGHT - SCREEN_HEIGHT) / 2
static int screen_offset_x = INITIAL_SCREEN_X;
static int screen_offset_y = INITIAL_SCREEN_Y;

static int arrow_yuv_width = 12;
static int arrow_yuv_height = 20;
static int arrow_yuv_lines[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 9, 7, 8, 8, 4, 4, 4, 4, 2, 0};
static int arrow_yuv_offsets[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 6, 6, 7, 0};
static uint8_t arrow_yuv[] = {
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0xeb, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0x10, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80,
    0xeb, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80,
    0xeb, 0x80, 0xeb, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80,
    0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0xeb, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0x10, 0x80, 0xeb, 0x80, 0xeb, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0xeb, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0xeb, 0x80,
    0xeb, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0xeb, 0x80,
    0xeb, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0xeb, 0x80, 0xeb, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0xeb, 0x80, 0xeb, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0xeb, 0x80, 0xeb, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80,
    0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80, 0x10, 0x80};
static void draw_arrow(void) {
    int relative_x = cursor_x + screen_offset_x;
    int relative_y = cursor_y + screen_offset_y;
    // NOTE on emu the cursor won't reach the right or bottom, and the top shows the bottom part of a cursor?
    if (relative_x < 0 || relative_y < 0 || relative_x >= SCREEN_FB_WIDTH || relative_y >= SCREEN_FB_HEIGHT - 20 /* NOTE why rsc-c does -20? */) {
        return;
    }

    for (int y = 0; y < arrow_yuv_height; y++) {
        int fb_index = (SCREEN_FB_WIDTH * 2 * (y + relative_y)) + (relative_x * 2);

        int arrow_offset = (arrow_yuv_offsets[y] * 2) + (arrow_yuv_width * 2 * y);

        int arrow_width = arrow_yuv_lines[y] * 2;

        memcpy((u8 *)xfb + fb_index + (arrow_yuv_offsets[y] * 2), arrow_yuv + arrow_offset, arrow_width);
    }
}

//---------------------------------------------------------------------------------
//	convert two RGB pixels to one Y1CbY2Cr.
//---------------------------------------------------------------------------------
static u32 rgb2yuv(u8 r1, u8 g1, u8 b1, u8 r2, u8 g2, u8 b2) {
    //---------------------------------------------------------------------------------
    int y1, cb1, cr1, y2, cb2, cr2, cb, cr;

    y1 = (299 * r1 + 587 * g1 + 114 * b1) / 1000;
    cb1 = (-16874 * r1 - 33126 * g1 + 50000 * b1 + 12800000) / 100000;
    cr1 = (50000 * r1 - 41869 * g1 - 8131 * b1 + 12800000) / 100000;
    y2 = (299 * r2 + 587 * g2 + 114 * b2) / 1000;
    cb2 = (-16874 * r2 - 33126 * g2 + 50000 * b2 + 12800000) / 100000;
    cr2 = (50000 * r2 - 41869 * g2 - 8131 * b2 + 12800000) / 100000;

    cb = (cb1 + cb2) >> 1;
    cr = (cr1 + cr2) >> 1;
    return (y1 << 24) | (cb << 16) | (y2 << 8) | cr;
}

int get_free_mem(void) {
    return SYS_GetArena1Hi() - SYS_GetArena1Lo() + SYS_GetArena2Hi() - SYS_GetArena2Lo();
}

bool platform_init(void) {
    // Initialise the video system
    VIDEO_Init();
    // Obtain the preferred video mode from the system
    // This will correspond to the settings in the Wii menu
    rmode = VIDEO_GetPreferredMode(NULL);

    // Allocate memory for the display in the uncached region
    // xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode)); // NOTE: causes green glitchy cursor
    xfb = SYS_AllocateFramebuffer(rmode);
    // VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK); // NOTE: why is this needed to avoid garbage fb on boot when rsc-c doesn't

    // Set up the video registers with the chosen mode
    VIDEO_Configure(rmode);

    // Tell the video hardware where our display memory is
    VIDEO_SetNextFramebuffer(xfb);

    // Make the display visible
    VIDEO_SetBlack(false);

    // Flush the video register changes to the hardware
    VIDEO_Flush();

    // Wait for Video setup to complete
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();

    // Initialise the console, required for printf
    SYS_STDIO_Report(true);

    if (!fatInitDefault()) {
        console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
        rs2_error("FAT init failed\n");
        VIDEO_Flush();
        return false;
    }
    return true;
}

void platform_new(int width, int height) {
    (void)width, (void)height;
    // This function initialises the attached controllers
    WPAD_Init();

    WPAD_SetDataFormat(0, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(0, rmode->fbWidth, rmode->xfbHeight);

    KEYBOARD_Init(NULL);
    MOUSE_Init();
    // settime(0); // would start gettime at 0, but why does this break on login?

    if (!_Client.lowmem) {
        AUDIO_Init(NULL);
        ASND_Init();
        ASND_Pause(0); // TODO move
    }
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
    for (int row = 0; row < pixmap->height; row++) {
        int screen_y = y + row + screen_offset_y;
        if (screen_y < 0)
            continue;
        if (screen_y >= SCREEN_FB_HEIGHT)
            break;
        for (int col = 0; col < pixmap->width; col += 2) {
            int screen_x = x + col + screen_offset_x;
            if (screen_x < 0)
                continue;
            if (screen_x >= SCREEN_FB_WIDTH)
                break;

            int dest_offset = screen_y * (rmode->fbWidth / 2) + (screen_x / 2);
            int src_offset = row * pixmap->width + col;

            int pixel1 = pixmap->pixels[src_offset];
            uint8_t r1 = (pixel1 >> 16) & 0xff;
            uint8_t g1 = (pixel1 >> 8) & 0xff;
            uint8_t b1 = pixel1 & 0xff;

            int pixel2 = (col + 1 < pixmap->width) ? pixmap->pixels[src_offset + 1] : pixel1;
            uint8_t r2 = (pixel2 >> 16) & 0xff;
            uint8_t g2 = (pixel2 >> 8) & 0xff;
            uint8_t b2 = pixel2 & 0xff;

            ((uint32_t *)xfb)[dest_offset] = rgb2yuv(r1, g1, b1, r2, g2, b2);
        }
    }

    draw_arrow();

    // VIDEO_Flush();
    // VIDEO_WaitVSync();
}
#define PAN_EDGE_DISTANCE 100
#define PAN_SPEED 10
#define DEADZONE 10
#define CENTER 128

void platform_poll_events(Client *c) {
    WPAD_ScanPads();
    WPADData *data = WPAD_Data(0);
    if (data->btns_d & WPAD_BUTTON_HOME) {
        exit(0);
    }
    if (data->ir.valid) {
        cursor_x = data->ir.x - screen_offset_x;
        cursor_y = data->ir.y - screen_offset_y;

        c->shell->idle_cycles = 0;
        c->shell->mouse_x = cursor_x;
        c->shell->mouse_y = cursor_y;

        if (_InputTracking.enabled) {
            inputtracking_mouse_moved(&_InputTracking, cursor_x, cursor_y);
        }
    }

    if (data->btns_d & WPAD_BUTTON_A || data->btns_d & WPAD_BUTTON_B) {
        c->shell->idle_cycles = 0;
        c->shell->mouse_click_x = cursor_x;
        c->shell->mouse_click_y = cursor_y;

        if (data->btns_d & WPAD_BUTTON_B) {
            c->shell->mouse_click_button = 2;
            c->shell->mouse_button = 2;
        } else {
            c->shell->mouse_click_button = 1;
            c->shell->mouse_button = 1;
        }

        if (_InputTracking.enabled) {
            inputtracking_mouse_pressed(&_InputTracking, cursor_x, cursor_y, data->btns_d & WPAD_BUTTON_B ? 1 : 0);
        }
    }
    if (data->btns_u & WPAD_BUTTON_A || data->btns_u & WPAD_BUTTON_B) {
        if (data->ir.valid) {
            c->shell->idle_cycles = 0;
            c->shell->mouse_button = 0;

            if (_InputTracking.enabled) {
                inputtracking_mouse_released(&_InputTracking, (data->btns_d & WPAD_BUTTON_B) != 0 ? 1 : 0);
            }
        }
    }

    if (data->exp.type == WPAD_EXP_NUNCHUK) {
        float stick_x = data->exp.nunchuk.js.pos.x;
        float stick_y = data->exp.nunchuk.js.pos.y;
        static bool ldown, rdown, udown, ddown;
        if (stick_x < CENTER - DEADZONE) {
            if (!ldown) {
                key_pressed(c->shell, K_LEFT, -1);
                ldown = true;
            }
        } else {
            if (ldown) {
                key_released(c->shell, K_LEFT, -1);
                ldown = false;
            }
        }
        if (stick_x > CENTER + DEADZONE) {
            if (!rdown) {
                key_pressed(c->shell, K_RIGHT, -1);
                rdown = true;
            }
        } else {
            if (rdown) {
                key_released(c->shell, K_RIGHT, -1);
                rdown = false;
            }
        }

        if (stick_y > CENTER + DEADZONE) {
            if (!udown) {
                key_pressed(c->shell, K_UP, -1);
                udown = true;
            }
        } else {
            if (udown) {
                key_released(c->shell, K_UP, -1);
                udown = false;
            }
        }
        if (stick_y < CENTER - DEADZONE) {
            if (!ddown) {
                key_pressed(c->shell, K_DOWN, -1);
                ddown = true;
            }
        } else {
            if (ddown) {
                key_released(c->shell, K_DOWN, -1);
                ddown = false;
            }
        }
    }

    if (data->btns_d & WPAD_BUTTON_LEFT) {
        key_pressed(c->shell, K_LEFT, -1);
    }
    if (data->btns_d & WPAD_BUTTON_RIGHT) {
        key_pressed(c->shell, K_RIGHT, -1);
    }
    if (data->btns_d & WPAD_BUTTON_UP) {
        key_pressed(c->shell, K_UP, -1);
    }
    if (data->btns_d & WPAD_BUTTON_DOWN) {
        key_pressed(c->shell, K_DOWN, -1);
    }

    if (data->btns_u & WPAD_BUTTON_LEFT) {
        key_released(c->shell, K_LEFT, -1);
    }
    if (data->btns_u & WPAD_BUTTON_RIGHT) {
        key_released(c->shell, K_RIGHT, -1);
    }
    if (data->btns_u & WPAD_BUTTON_UP) {
        key_released(c->shell, K_UP, -1);
    }
    if (data->btns_u & WPAD_BUTTON_DOWN) {
        key_released(c->shell, K_DOWN, -1);
    }

    // TODO: why do nunchuck btn releases only show up some of the time?!
    if (data->btns_d & WPAD_BUTTON_MINUS /* || (data->exp.type == WPAD_EXP_NUNCHUK && (data->exp.nunchuk.btns << 16) & WPAD_NUNCHUK_BUTTON_C) */) {
        key_pressed(c->shell, K_CONTROL, -1);
    }

    if (data->btns_u & WPAD_BUTTON_MINUS /* || (data->exp.type == WPAD_EXP_NUNCHUK && (data->exp.nunchuk.btns_released << 16) & WPAD_NUNCHUK_BUTTON_C) */) {
        key_released(c->shell, K_CONTROL, -1);
    }

    static bool pan = false;
    if (data->btns_d & WPAD_BUTTON_PLUS /* || (data->exp.type == WPAD_EXP_NUNCHUK && (data->exp.nunchuk.btns << 16) & WPAD_NUNCHUK_BUTTON_Z) */) {
        pan = true;
    }

    if (data->btns_u & WPAD_BUTTON_PLUS /* || (data->exp.type == WPAD_EXP_NUNCHUK && (data->exp.nunchuk.btns_released << 16) & WPAD_NUNCHUK_BUTTON_Z) */) {
        pan = false;
    }

    if (data->btns_d & WPAD_BUTTON_1) {
        screen_offset_x = INITIAL_SCREEN_X;
        screen_offset_y = INITIAL_SCREEN_Y;
        c->redraw_background = true;
    }

    // unused, maybe open up a keyboard?
    if (data->btns_d & WPAD_BUTTON_2) {
    }

    if (pan) {
        if (cursor_x < PAN_EDGE_DISTANCE - screen_offset_x) {
            if (screen_offset_x < 0) {
                screen_offset_x = MIN(0, screen_offset_x + PAN_SPEED);
                c->redraw_background = true;
            }
        }

        if (cursor_x > SCREEN_FB_WIDTH - (PAN_EDGE_DISTANCE - screen_offset_x)) {
            if (screen_offset_x > SCREEN_FB_WIDTH - SCREEN_WIDTH) {
                screen_offset_x = MAX(SCREEN_FB_WIDTH - SCREEN_WIDTH, screen_offset_x - PAN_SPEED);
                c->redraw_background = true;
            }
        }

        if (cursor_y < PAN_EDGE_DISTANCE - screen_offset_y) {
            if (screen_offset_y < 0) {
                screen_offset_y = MIN(0, screen_offset_y + PAN_SPEED);
                c->redraw_background = true;
            }
        }

        if (cursor_y > SCREEN_FB_HEIGHT - (PAN_EDGE_DISTANCE - screen_offset_y)) {
            if (screen_offset_y > SCREEN_FB_HEIGHT - SCREEN_HEIGHT) {
                screen_offset_y = MAX(SCREEN_FB_HEIGHT - SCREEN_HEIGHT, screen_offset_y - PAN_SPEED);
                c->redraw_background = true;
            }
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
    uint64_t ticks = gettime();
    return (uint64_t)(ticks / TB_TIMER_CLOCK);
}
void delay_ticks(int ticks) {
    struct timespec elapsed, tv;
    elapsed.tv_sec = ticks / 1000;
    elapsed.tv_nsec = (ticks % 1000) * 1000000;
    tv.tv_sec = elapsed.tv_sec;
    tv.tv_nsec = elapsed.tv_nsec;
    nanosleep(&tv, &elapsed);

    /* uint64_t end = get_ticks() + ticks;

    while (get_ticks() != end)
        ; */
}
#endif
