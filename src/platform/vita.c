#if defined(__vita__) && (!defined(SDL) || SDL == 0)
#include <malloc.h>
#include <string.h>

#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/touch.h>

#include "../client.h"
#include "../custom.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../pixmap.h"
#include "../platform.h"

extern ClientData _Client;
extern InputTracking _InputTracking;
extern Custom _Custom;

#define SCREEN_FB_SIZE (2 * 1024 * 1024) // Must be 256KB aligned
static SceUID displayblock;
static void *base; // pointer to frame buffer
static int mutex;
static int xoff = (SCREEN_FB_WIDTH - SCREEN_WIDTH) / 2;

static SceTouchData touch[SCE_TOUCH_PORT_MAX_NUM], touch_old[SCE_TOUCH_PORT_MAX_NUM];
static SceCtrlData ctrl, ctrl_old;

bool platform_init(void) {
    return true;
}

void platform_new(GameShell *shell) {
    (void)shell;
    mutex = sceKernelCreateMutex("fb_mutex", 0, 0, NULL);
    displayblock = sceKernelAllocMemBlock("display", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, SCREEN_FB_SIZE, NULL);
    if (displayblock < 0)
        return;
    sceKernelGetMemBlockBase(displayblock, (void **)&base);
    SceDisplayFrameBuf frame = {sizeof(frame), base, (SCREEN_FB_WIDTH), 0, SCREEN_FB_WIDTH, SCREEN_FB_HEIGHT};
    sceDisplaySetFrameBuf(&frame, SCE_DISPLAY_SETBUF_NEXTFRAME);

    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    // sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);

    // sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITAL);
}

void platform_free(void) {
    sceKernelDeleteMutex(mutex);
    sceDisplaySetFrameBuf(NULL, SCE_DISPLAY_SETBUF_IMMEDIATE);
    sceKernelFreeMemBlock(displayblock);
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
    static bool right_click = false;

    static SceTouchPanelInfo info;
    sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &info);

    memcpy(touch_old, touch, sizeof(touch_old));

    for (int port = 0; port < 1 /* SCE_TOUCH_PORT_MAX_NUM */; port++) {
        sceTouchPeek(port, &touch[port], 1);
        for (int i = 0; i < 1 /* SCE_TOUCH_MAX_REPORT */; i++) {
            static bool touch_down = false;
            if (touch[port].reportNum > 0) {
                int x = touch[port].report[i].x * SCREEN_FB_WIDTH / info.maxAaX - xoff;
                int y = touch[port].report[i].y * SCREEN_FB_HEIGHT / info.maxAaY;

                c->shell->idle_cycles = 0;
                c->shell->mouse_x = x;
                c->shell->mouse_y = y;

                if (_InputTracking.enabled) {
                    inputtracking_mouse_moved(&_InputTracking, x, y);
                }

                if (!touch_down) {
                    touch_down = true;
                    update_touch = true;

                    last_touch_x = x;
                    last_touch_y = y;

                    if (insideMobileInputArea(c)) {
                        // SDL_StartTextInput();
                    }

                    if (right_click) {
                        last_touch_button = 2;
                        c->shell->mouse_button = 2;
                    } else {
                        last_touch_button = 1;
                        c->shell->mouse_button = 1;
                    }

                    if (_InputTracking.enabled) {
                        inputtracking_mouse_pressed(&_InputTracking, x, y, right_click ? 1 : 0);
                    }
                }
            } else {
                if (touch_down) {
                    touch_down = false;

                    c->shell->idle_cycles = 0;
                    c->shell->mouse_button = 0;

                    if (_InputTracking.enabled) {
                        inputtracking_mouse_released(&_InputTracking, right_click ? 1 : 0);
                    }
                }
            }
        }
    }

    ctrl_old = ctrl;
    sceCtrlPeekBufferPositive(0, &ctrl, 1);

    int pressed = ctrl.buttons & ~ctrl_old.buttons;
    int released = ~ctrl.buttons & ctrl_old.buttons;

    if (pressed & SCE_CTRL_TRIANGLE) {
        key_pressed(c->shell, K_CONTROL, -1);
    }

    if (pressed & SCE_CTRL_CIRCLE) {
    }

    if (pressed & SCE_CTRL_CROSS) {
        right_click = true;
    }

    if (pressed & SCE_CTRL_SQUARE) {
    }

    if (pressed & SCE_CTRL_DOWN) {
        key_pressed(c->shell, K_DOWN, -1);
    }

    if (pressed & SCE_CTRL_LEFT) {
        key_pressed(c->shell, K_LEFT, -1);
    }

    if (pressed & SCE_CTRL_UP) {
        key_pressed(c->shell, K_UP, -1);
    }

    if (pressed & SCE_CTRL_RIGHT) {
        key_pressed(c->shell, K_RIGHT, -1);
    }

    if (pressed & SCE_CTRL_SELECT) {
        _Custom.showPerformance = !_Custom.showPerformance;
    }
    if (pressed & SCE_CTRL_START) {
    }
    if (pressed & SCE_CTRL_L1) {
    }
    if (pressed & SCE_CTRL_R1) {
    }

    if (released & SCE_CTRL_TRIANGLE) {
        key_released(c->shell, K_CONTROL, -1);
    }

    if (released & SCE_CTRL_CIRCLE) {
    }

    if (released & SCE_CTRL_CROSS) {
        right_click = false;
    }

    if (released & SCE_CTRL_SQUARE) {
    }

    if (released & SCE_CTRL_DOWN) {
        key_released(c->shell, K_DOWN, -1);
    }

    if (released & SCE_CTRL_LEFT) {
        key_released(c->shell, K_LEFT, -1);
    }

    if (released & SCE_CTRL_UP) {
        key_released(c->shell, K_UP, -1);
    }

    if (released & SCE_CTRL_RIGHT) {
        key_released(c->shell, K_RIGHT, -1);
    }

    if (released & SCE_CTRL_SELECT) {
    }
    if (released & SCE_CTRL_START) {
    }
    if (released & SCE_CTRL_L1) {
    }
    if (released & SCE_CTRL_R1) {
    }

    // rs2_log("\e[m Stick:[%3i:%3i][%3i:%3i]\r", ctrl.lx,ctrl.ly, ctrl.rx,ctrl.ry);
}
void platform_blit_surface(Surface *surface, int x, int y) {
    x += xoff;

    sceKernelLockMutex(mutex, 1, NULL);
    platform_set_pixels(base, surface, x, y, true);
    sceKernelUnlockMutex(mutex, 1);
}
void platform_update_surface(void) {
}
uint64_t rs2_now(void) {
    return sceKernelGetSystemTimeWide() / 1000;
}
void rs2_sleep(int ms) {
    sceKernelDelayThreadCB(ms * 1000);
}
#endif
