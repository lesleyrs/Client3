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

#define CURSOR_W 12
#define CURSOR_H 18
static const unsigned char cursor[] = {
    0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x01, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x01, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x01, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x01, 0xff,
    0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x01, 0xff,
    0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x01, 0xff,
    0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x01, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x01, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x01, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x01, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

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
    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(NULL);

    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK); // NOTE: why is this needed to avoid garbage fb on boot when rsc-c doesn't

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();

    SYS_STDIO_Report(true);

    if (!fatInitDefault()) {
        console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
        rs2_error("FAT init failed\n");
        VIDEO_Flush();
        return false;
    }

    WPAD_Init();

    WPAD_SetDataFormat(0, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(0, rmode->fbWidth, rmode->xfbHeight);

    KEYBOARD_Init(NULL);
    MOUSE_Init();

    if (!_Client.lowmem) {
        AUDIO_Init(NULL);
        ASND_Init();
        ASND_Pause(0); // TODO move
    }

    return true;
}

void platform_new(int width, int height) {
    (void)width, (void)height;
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
    int relative_x = cursor_x + screen_offset_x;
    int relative_y = cursor_y + screen_offset_y;

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
            uint8_t r1, g1, b1, r2, g2, b2;

            // draw cursor in here + redraw_background in poll_events
            int cx = screen_x - relative_x;
            int cy = screen_y - relative_y;
            if (cx >= 0 && cy >= 0 && cx < CURSOR_W && cy < CURSOR_H) {
                int i = (cy * CURSOR_W + cx) * 4;
                const uint8_t *pixel1_alpha = &cursor[i + 3];
                const uint8_t *pixel2_alpha = &cursor[i + 7];

                if (*pixel1_alpha > 0) {
                    r1 = cursor[i];
                    g1 = cursor[i + 1];
                    b1 = cursor[i + 2];

                    if (*pixel2_alpha > 0) {
                        r2 = cursor[i + 4];
                        g2 = cursor[i + 5];
                        b2 = cursor[i + 6];
                    } else {
                        r2 = g2 = b2 = 0;
                    }

                    ((uint32_t *)xfb)[dest_offset] = rgb2yuv(r1, g1, b1, r2, g2, b2);
                    continue;
                }
            }

            int pixel1 = pixmap->pixels[src_offset];
            r1 = (pixel1 >> 16) & 0xff;
            g1 = (pixel1 >> 8) & 0xff;
            b1 = pixel1 & 0xff;

            int pixel2 = (col + 1 < pixmap->width) ? pixmap->pixels[src_offset + 1] : pixel1;
            r2 = (pixel2 >> 16) & 0xff;
            g2 = (pixel2 >> 8) & 0xff;
            b2 = pixel2 & 0xff;

            ((uint32_t *)xfb)[dest_offset] = rgb2yuv(r1, g1, b1, r2, g2, b2);
        }
    }
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

    static float ir_x = 0;
    static float ir_y = 0;
    if (data->ir.valid && (data->ir.x != ir_x || data->ir.y != ir_y)) {
        // NOTE: causes lag if moving wiimote/cursor too much
        c->redraw_background = true;
        ir_x = data->ir.x;
        ir_y = data->ir.y;

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
void platform_blit_surface(int x, int y, int w, int h, Surface *surface) {
}
void platform_update_surface(void) {
}
void platform_draw_rect(int x, int y, int w, int h, int color) {
}
void platform_fill_rect(int x, int y, int w, int h, int color) {
}
uint64_t get_ticks(void) {
    // NOTE: gettime() starts at high value, maybe subtract the initial val
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
