#ifdef __PSP__
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <pspnet.h>
#include <pspnet_apctl.h>
#include <pspnet_inet.h>
#include <pspnet_resolver.h>
#include <psppower.h>
#include <psprtc.h>
#include <psputility.h>

#include "../client.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../pixmap.h"
#include "../platform.h"

extern ClientData _Client;
extern InputTracking _InputTracking;

PSP_MODULE_INFO("client", 0, 2, 225);
// PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

#undef SCREEN_WIDTH
#undef SCREEN_HEIGHT

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272

#define VRAM_STRIDE 512

static uint16_t *fb = (uint16_t *)0x04000000; // vram start
static SceCtrlData pad, last_pad;
static int cursor_x = SCREEN_WIDTH / 2;
static int cursor_y = SCREEN_HEIGHT / 2;

#include <pspsdk.h>
int get_free_mem(void) {
    return pspSdkTotalFreeUserMemSize();
}
int get_cursor_x(void) {
    return cursor_x;
}
int get_cursor_y(void) {
    return cursor_y;
}

int exit_callback(int arg1, int arg2, void *common) {
    sceKernelExitGame();
    return 0;
}

/* Callback thread */
int CallbackThread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

/* Sets up the callback thread and returns its thread id */
int SetupCallbacks(void) {
    int thid = 0;
    thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, 0);
    }
    return thid;
}

void platform_init(void) {
    // NOTE maybe re-add throwError code from sample? look where stdout/stderr goes in emulator
    pspDebugScreenInit();
    SetupCallbacks();
    scePowerSetClockFrequency(333, 333, 166);
}

void platform_new(GameShell *shell, int width, int height) {
    (void)shell, (void)width, (void)height;
    sceDisplaySetFrameBuf(fb, VRAM_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_NEXTFRAME);
    // TODO rm
    rs2_log("%d\n", sceCtrlGetSamplingCycle);

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    sceUtilityLoadNetModule(PSP_NET_MODULE_INET);

    int res;
    res = sceNetInit(64 * 1024, 32, 2 * 1024, 32, 2 * 1024);
    res = sceNetInetInit();
    res = sceNetResolverInit();

    sceNetApctlInit(0x2000, 20);
    sceNetApctlConnect(1);

    // TODO: check possible issue with multiple saved access points
    int apctl_status = 0;
    int last_status = -1;
    while (apctl_status != PSP_NET_APCTL_STATE_GOT_IP) {
        sceNetApctlGetState(&apctl_status);
        if (apctl_status != last_status) {
            pspDebugScreenPrintf("connection state %d of 4\n", apctl_status);
            last_status = apctl_status;
        }
        delay_ticks(50); // Needs to have a delay. Otherwise fails.
    }
}

void platform_free(GameShell *shell) {
    sceNetApctlTerm();
    sceNetResolverTerm();
    sceNetInetTerm();
    sceNetTerm();
    sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
    sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
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
    uint16_t *fb_ptr = (uint16_t *)fb + (y * VRAM_STRIDE + x);
    for (int row = 0; row < pixmap->height; row++) {
        if (y + row >= SCREEN_HEIGHT)
            break;

        for (int col = 0; col < pixmap->width; col++) {
            if (x + col >= SCREEN_WIDTH)
                break;

            int src_offset = row * pixmap->width + col;

            int pixel = pixmap->pixels[src_offset];
            uint8_t b = (pixel >> 16) & 0xff;
            uint8_t g = (pixel >> 8) & 0xff;
            uint8_t r = pixel & 0xff;

            uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);

            fb_ptr[row * VRAM_STRIDE + col] = rgb565;
        }
    }
    // sceDisplayWaitVblankStart();
    // sceDisplaySetFrameBuf(fb, VRAM_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_NEXTFRAME);
}

void platform_get_keycodes(Keysym *keysym, int *code, char *ch) {
}

void platform_poll_events(Client *c) {
    sceCtrlPeekBufferPositive(&pad, 1);

    if (!(last_pad.Buttons & PSP_CTRL_LEFT) && pad.Buttons & PSP_CTRL_LEFT) {
        key_pressed(c->shell, K_LEFT, -1);
    }

    if (!(last_pad.Buttons & PSP_CTRL_RIGHT) && pad.Buttons & PSP_CTRL_RIGHT) {
        key_pressed(c->shell, K_RIGHT, -1);
    }

    if (!(last_pad.Buttons & PSP_CTRL_UP) && pad.Buttons & PSP_CTRL_UP) {
        key_pressed(c->shell, K_UP, -1);
    }

    if (!(last_pad.Buttons & PSP_CTRL_DOWN) && pad.Buttons & PSP_CTRL_DOWN) {
        key_pressed(c->shell, K_DOWN, -1);
    }

    if (last_pad.Buttons & PSP_CTRL_LEFT && !(pad.Buttons & PSP_CTRL_LEFT)) {
        key_released(c->shell, K_LEFT, -1);
    }

    if (last_pad.Buttons & PSP_CTRL_RIGHT && !(pad.Buttons & PSP_CTRL_RIGHT)) {
        key_released(c->shell, K_RIGHT, -1);
    }

    if (last_pad.Buttons & PSP_CTRL_UP && !(pad.Buttons & PSP_CTRL_UP)) {
        key_released(c->shell, K_UP, -1);
    }

    if (last_pad.Buttons & PSP_CTRL_DOWN && !(pad.Buttons & PSP_CTRL_DOWN)) {
        key_released(c->shell, K_DOWN, -1);
    }

    bool ctrl_press = !(last_pad.Buttons & PSP_CTRL_TRIANGLE) && pad.Buttons & PSP_CTRL_TRIANGLE;
    bool ctrl_release = last_pad.Buttons & PSP_CTRL_TRIANGLE && !(pad.Buttons & PSP_CTRL_TRIANGLE);
    if (ctrl_press) {
        key_pressed(c->shell, K_CONTROL, -1);
    }

    if (ctrl_release) {
        key_released(c->shell, K_CONTROL, -1);
    }

    if (!(last_pad.Buttons & PSP_CTRL_SQUARE) && pad.Buttons & PSP_CTRL_SQUARE) {
        if (!c->ingame) {
            client_login(c, c->username, c->password, false);
        }
    }

    if (pad.Lx != 128 || pad.Ly != 128) {
        // TODO allow changing cursor sensitivity
        cursor_x += (pad.Lx - 128) / 20;
        cursor_y += (pad.Ly - 128) / 20;

        if (cursor_x < 0 || cursor_x > SCREEN_WIDTH) {
            cursor_x = MAX(0, MIN(cursor_x, SCREEN_WIDTH));
        }
        if (cursor_y < 0 || cursor_y > SCREEN_HEIGHT) {
            cursor_y = MAX(0, MIN(cursor_y, SCREEN_HEIGHT));
        }

        int x = cursor_x;
        int y = cursor_y;

        c->shell->idle_cycles = 0;
        c->shell->mouse_x = x;
        c->shell->mouse_y = y;

        if (_InputTracking.enabled) {
            inputtracking_mouse_moved(&_InputTracking, x, y);
        }
    }

    bool left_press = !(last_pad.Buttons & PSP_CTRL_CIRCLE) && pad.Buttons & PSP_CTRL_CIRCLE;
    bool right_press = !(last_pad.Buttons & PSP_CTRL_CROSS) && pad.Buttons & PSP_CTRL_CROSS;

    if (left_press || right_press) {
        int x = cursor_x;
        int y = cursor_y;

        c->shell->idle_cycles = 0;
        c->shell->mouse_click_x = x;
        c->shell->mouse_click_y = y;

        if (right_press) {
            c->shell->mouse_click_button = 2;
            c->shell->mouse_button = 2;
        } else {
            c->shell->mouse_click_button = 1;
            c->shell->mouse_button = 1;
        }

        if (_InputTracking.enabled) {
            inputtracking_mouse_pressed(&_InputTracking, x, y, right_press ? 1 : 0);
        }
    }

    bool left_release = last_pad.Buttons & PSP_CTRL_CIRCLE && !(pad.Buttons & PSP_CTRL_CIRCLE);
    bool right_release = last_pad.Buttons & PSP_CTRL_CROSS && !(pad.Buttons & PSP_CTRL_CROSS);

    if (left_release || right_release) {
        c->shell->idle_cycles = 0;
        c->shell->mouse_button = 0;

        if (_InputTracking.enabled) {
            inputtracking_mouse_released(&_InputTracking, right_release ? 1 : 0);
        }
    }

    last_pad = pad;
}
void platform_update_surface(GameShell *shell) {
}
void platform_fill_rect(GameShell *shell, int x, int y, int w, int h, int color) {
}
void platform_blit_surface(GameShell *shell, int x, int y, int w, int h, Surface *surface) {
}
uint64_t get_ticks(void) {
    return sceKernelGetSystemTimeWide() / 1000;
}
void delay_ticks(int ticks) {
    sceKernelDelayThreadCB(ticks * 1000);
    /* uint64_t end = get_ticks() + ticks;

    while (get_ticks() != end)
        ; */
}
#endif
