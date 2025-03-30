#if defined(_arch_dreamcast)
#include <arch/arch.h>
#include <dc/biosfont.h>
#include <dc/video.h>
#include <kos.h>

#include "../client.h"
#include "../custom.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../pixmap.h"
#include "../platform.h"

extern ClientData _Client;
extern InputTracking _InputTracking;
extern Custom _Custom;

static uint32_t prev_btns = 0;
static int cursor_x = SCREEN_FB_WIDTH / 2;
static int cursor_y = SCREEN_FB_HEIGHT / 2;
#define INITIAL_SCREEN_X (SCREEN_FB_WIDTH - SCREEN_WIDTH) / 2
#define INITIAL_SCREEN_Y (SCREEN_FB_HEIGHT - SCREEN_HEIGHT) / 2
static int screen_offset_x = INITIAL_SCREEN_X;
static int screen_offset_y = INITIAL_SCREEN_Y;

bool platform_init(void) {
    vid_set_mode(DM_640x480, PM_RGB888);

    if (!DBL_MEM) {
        int off = (SCREEN_FB_WIDTH * BFONT_HEIGHT) + BFONT_THIN_WIDTH;
        bfont_draw_str(vram_s + off, SCREEN_FB_WIDTH, 1, "Unsupported: 16MB RAM detected, need 32MB expansion\n");
        return false;
    }

    chdir("/cd");

    // TODO rm, waiting for path dot extension fix
    file_t d;
    dirent_t *de;

    rs2_log("Reading directory from CD-Rom:\r\n");

    /* Read and print the root directory */
    d = fs_open("/cd/cache/client", O_RDONLY | O_DIR);

    if (d == 0) {
        rs2_error("Can't open root!\r\n");
        return false;
    }

    while ((de = fs_readdir(d))) {
        rs2_log("%s  /  ", de->name);

        if (de->size >= 0) {
            rs2_log("%d\r\n", de->size);
        } else {
            rs2_log("DIR\r\n");
        }
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
    for (int h = 0; h < pixmap->height; h++) {
        int screen_y = y + h + screen_offset_y;
        if (screen_y < 0)
            continue;
        if (screen_y >= SCREEN_FB_HEIGHT)
            break;
        for (int w = 0; w < pixmap->width; w++) {
            int screen_x = x + w + screen_offset_x;
            if (screen_x < 0)
                continue;
            if (screen_x >= SCREEN_FB_WIDTH)
                break;
            vram_l[(screen_y)*SCREEN_FB_WIDTH + (screen_x)] = pixmap->pixels[h * pixmap->width + w];
        }
    }
}

void platform_poll_events(Client *c) {
    maple_device_t *cont;
    cont_state_t *state;

    cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);

    if (!cont)
        return;

    state = (cont_state_t *)maple_dev_status(cont);

    if (!state) {
        return;
    }

    if (state->buttons & CONT_DPAD_UP) {
        key_pressed(c->shell, K_UP, -1);
    }

    if (state->buttons & CONT_DPAD_DOWN) {
        key_pressed(c->shell, K_DOWN, -1);
    }

    if (state->buttons & CONT_DPAD_LEFT) {
        key_pressed(c->shell, K_LEFT, -1);
    }

    if (state->buttons & CONT_DPAD_RIGHT) {
        key_pressed(c->shell, K_RIGHT, -1);
    }

    uint32_t released = prev_btns & ~state->buttons;
    uint32_t pressed = ~prev_btns & state->buttons;

    if (released & CONT_DPAD_UP) {
        key_released(c->shell, K_UP, -1);
    }

    if (released & CONT_DPAD_DOWN) {
        key_released(c->shell, K_DOWN, -1);
    }

    if (released & CONT_DPAD_LEFT) {
        key_released(c->shell, K_LEFT, -1);
    }

    if (released & CONT_DPAD_RIGHT) {
        key_released(c->shell, K_RIGHT, -1);
    }

    if (pressed & CONT_Y) {
        key_pressed(c->shell, K_CONTROL, -1);
    }

    if (released & CONT_Y) {
        key_released(c->shell, K_CONTROL, -1);
    }

    if (pressed & CONT_X) {
    }

    // TODO allow setting "cursor" sensitivity
    cursor_x = MAX(0, MIN(cursor_x + (state->joyx >> 4), SCREEN_WIDTH - 1));
    cursor_y = MAX(0, MIN(cursor_y + (state->joyy >> 4), SCREEN_HEIGHT - 1));

    if (state->ltrig) {
        screen_offset_x = INITIAL_SCREEN_X;
        screen_offset_y = INITIAL_SCREEN_Y;
        c->redraw_background = true;
    }
    if (state->rtrig) {
        screen_offset_x = MAX(SCREEN_FB_WIDTH - SCREEN_WIDTH, MIN(screen_offset_x - (state->joyx >> 4), 0));
        screen_offset_y = MAX(SCREEN_FB_HEIGHT - SCREEN_HEIGHT, MIN(screen_offset_y - (state->joyy >> 4), 0));
        c->redraw_background = true;
    }
    // TODO see if it becomes exactly 0 on real hw too
    if (state->joyx || state->joyy) {
        int x = cursor_x;
        int y = cursor_y;

        c->shell->idle_cycles = 0;
        c->shell->mouse_x = x;
        c->shell->mouse_y = y;

        if (_InputTracking.enabled) {
            inputtracking_mouse_moved(&_InputTracking, x, y);
        }
    }

    if (pressed & CONT_A || pressed & CONT_B) {
        int x = cursor_x;
        int y = cursor_y;

        c->shell->idle_cycles = 0;
        c->shell->mouse_click_x = x;
        c->shell->mouse_click_y = y;

        if (pressed & CONT_A) {
            c->shell->mouse_click_button = 2;
            c->shell->mouse_button = 2;
        } else {
            c->shell->mouse_click_button = 1;
            c->shell->mouse_button = 1;
        }

        if (_InputTracking.enabled) {
            inputtracking_mouse_pressed(&_InputTracking, x, y, pressed & CONT_A ? 1 : 0);
        }
    }
    if (released & CONT_A || released & CONT_B) {
        c->shell->idle_cycles = 0;
        c->shell->mouse_button = 0;

        if (_InputTracking.enabled) {
            inputtracking_mouse_released(&_InputTracking, (released & CONT_A) != 0 ? 1 : 0);
        }
    }

    prev_btns = state->buttons;
}
void platform_update_surface(void) {
}
void platform_fill_rect(int x, int y, int w, int h, int color) {
}
void platform_blit_surface(int x, int y, int w, int h, Surface *surface) {
}
uint64_t get_ticks(void) {
    return timer_ms_gettime64();
}
void delay_ticks(int ticks) {
    thd_sleep(ticks);
}
#endif
