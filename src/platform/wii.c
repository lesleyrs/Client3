#ifdef __WII__
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <asndlib.h>
#include <gccore.h>
#include <ogc/usbmouse.h>
#include <wiikeyboard/keyboard.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <ogc/lwp_watchdog.h>
#include <ogcsys.h>

#include "../client.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../pixmap.h"
#include "../platform.h"
#include "../pix2d.h"

#undef SCREEN_WIDTH
#undef SCREEN_HEIGHT

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

extern ClientData _Client;
extern InputTracking _InputTracking;

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;
static int wii_mouse_x = 0;
static int wii_mouse_y = 0;

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
    if (wii_mouse_x >= SCREEN_WIDTH || wii_mouse_y >= SCREEN_HEIGHT - 20 /* NOTE ? */) {
        return;
    }

    for (int y = 0; y < arrow_yuv_height; y++) {
        int fb_index = (SCREEN_WIDTH * 2 * (y + wii_mouse_y)) + (wii_mouse_x * 2);

        int arrow_offset = (arrow_yuv_offsets[y] * 2) + (arrow_yuv_width * 2 * y);

        int arrow_width = arrow_yuv_lines[y] * 2;

        memcpy((u8*)xfb + fb_index + (arrow_yuv_offsets[y] * 2), arrow_yuv + arrow_offset, arrow_width);
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

void platform_init(void) {
    if (!fatInitDefault()) {
        rs2_error("FAT init failed\n");
    }
}

void platform_new(GameShell *shell, int width, int height) {
    (void)shell, (void)width, (void)height;
    // Initialise the video system
    VIDEO_Init();

    // This function initialises the attached controllers
    WPAD_Init();

    if (!_Client.lowmem) {
        AUDIO_Init(NULL);
        ASND_Init();
        ASND_Pause(0); // TODO move
    }

    // Obtain the preferred video mode from the system
    // This will correspond to the settings in the Wii menu
    rmode = VIDEO_GetPreferredMode(NULL);

    // Allocate memory for the display in the uncached region
    // xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode)); // NOTE: causes green glitchy cursor
    xfb = SYS_AllocateFramebuffer(rmode);
    // VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK); // NOTE: why is this needed to avoid garbage fb on boot when rsc-c doesn't

    // Initialise the console, required for printf
    // console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    SYS_STDIO_Report(true);

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

    WPAD_SetDataFormat(0, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(0, rmode->fbWidth, rmode->xfbHeight);

    KEYBOARD_Init(NULL);
    MOUSE_Init();
    // settime(0); // would start gettime at 0, but why does this break on login?
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
    for (int row = 0; row < pixmap->height; row++) {
        if (y + row >= SCREEN_HEIGHT)
            break;

        for (int col = 0; col < pixmap->width; col += 2) {
            if (x + col >= SCREEN_WIDTH)
                break;

            int src_offset = row * pixmap->width + col;
            int dest_offset = (y + row) * (rmode->fbWidth / 2) + (x + col) / 2;

            int pixel1 = pixmap->pixels[src_offset];
            uint8_t r1 = (pixel1 >> 16) & 0xff;
            uint8_t g1 = (pixel1 >> 8) & 0xff;
            uint8_t b1 = pixel1 & 0xff;

            int pixel2 = pixmap->pixels[src_offset + 1];
            uint8_t r2 = (pixel2 >> 16) & 0xff;
            uint8_t g2 = (pixel2 >> 8) & 0xff;
            uint8_t b2 = pixel2 & 0xff;

            // TODO is this conversion ok? some pixels seem off on left side of the inventory
            ((uint32_t *)xfb)[dest_offset] = rgb2yuv(r1, g1, b1, r2, g2, b2);
        }
    }

    // TODO
    // draw_arrow();

    // VIDEO_Flush();
    // VIDEO_WaitVSync();
}
void platform_get_keycodes(Keysym *keysym, int *code, char *ch) {
}
#define K_LEFT 37
#define K_RIGHT 39
#define K_UP 38
#define K_DOWN 40

void platform_poll_events(Client *c) {
    WPAD_ScanPads();
    WPADData *data = WPAD_Data(0);
    if (data->btns_d & WPAD_BUTTON_HOME) {
        exit(0);
    }
    if (data->ir.valid) {
        wii_mouse_x = data->ir.x;
        wii_mouse_y = data->ir.y;

        c->shell->idle_cycles = 0;
        c->shell->mouse_x = wii_mouse_x;
        c->shell->mouse_y = wii_mouse_y;

        if (_InputTracking.enabled) {
            inputtracking_mouse_moved(&_InputTracking, wii_mouse_x, wii_mouse_y);
        }
    }

    if (data->btns_d & WPAD_BUTTON_A || data->btns_d & WPAD_BUTTON_B) {
        c->shell->idle_cycles = 0;
        c->shell->mouse_click_x = wii_mouse_x;
        c->shell->mouse_click_y = wii_mouse_y;

        if (data->btns_d & WPAD_BUTTON_B) {
            c->shell->mouse_click_button = 2;
            c->shell->mouse_button = 2;
        } else {
            c->shell->mouse_click_button = 1;
            c->shell->mouse_button = 1;
        }

        if (_InputTracking.enabled) {
            inputtracking_mouse_pressed(&_InputTracking, wii_mouse_x, wii_mouse_y, data->btns_d & WPAD_BUTTON_B ? 1 : 0);
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
}
void platform_update_surface(GameShell *shell) {
}
void platform_fill_rect(GameShell *shell, int x, int y, int w, int h, int color) {
}
void platform_blit_surface(GameShell *shell, int x, int y, int w, int h, Surface *surface) {
}
uint64_t get_ticks(void) {
    uint64_t ticks = gettime();
    return (uint64_t)(ticks / TB_TIMER_CLOCK);
}
void delay_ticks(int ticks) {
    uint64_t end = get_ticks() + ticks;

    while (get_ticks() != end)
        ;
}
#endif
