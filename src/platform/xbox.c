#if defined(NXDK)
#include <hal/debug.h>
#include <hal/xbox.h>
#include <nxdk/mount.h>
#include <nxdk/net.h>
#include <windows.h>

#include <SDL.h>
#include <hal/video.h>
#include <stdlib.h>

#include "../client.h"
#include "../custom.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../pixmap.h"
#include "../platform.h"

extern ClientData _Client;
extern InputTracking _InputTracking;
extern Custom _Custom;

volatile static uint32_t *rgbx;

static int cursor_x = SCREEN_FB_WIDTH / 2;
static int cursor_y = SCREEN_FB_HEIGHT / 2;
#define INITIAL_SCREEN_X (SCREEN_FB_WIDTH - SCREEN_WIDTH) / 2
#define INITIAL_SCREEN_Y (SCREEN_FB_HEIGHT - SCREEN_HEIGHT) / 2
static int screen_offset_x = INITIAL_SCREEN_X;
static int screen_offset_y = INITIAL_SCREEN_Y;

#define CURSOR_W 12
#define CURSOR_H 18
#define CURSOR_SENSITIVITY 5000

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

SDL_GameController *pad = NULL;

bool platform_init(void) {
    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
    rgbx = (uint32_t *)XVideoGetFB();

    /* debugPrint("Content of D:\\\n");

    WIN32_FIND_DATA findFileData;
    HANDLE hFind;

    // Like on Windows, "*.*" and "*" will both list all files,
    // no matter whether they contain a dot or not
    hFind = FindFirstFile("D:\\*.*", &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        debugPrint("FindFirstHandle() failed!\n");
        Sleep(5000);
        return 1;
    }

    do {
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            debugPrint("Directory: ");
        } else {
            debugPrint("File     : ");
        }

        debugPrint("%s\n", findFileData.cFileName);
    } while (FindNextFile(hFind, &findFileData) != 0);

    debugPrint("\n");

    DWORD error = GetLastError();
    if (error == ERROR_NO_MORE_FILES) {
        debugPrint("Done!\n");
    } else {
        debugPrint("error: %lx\n", error);
    }

    FindClose(hFind);

    while (1) {
        Sleep(2000);
    } */

    if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
        debugPrint("SDL_Init failed: %s\n", SDL_GetError());
    }
    return true;
}

void platform_new(GameShell *shell) {
    (void)shell;
}

void platform_free(void) {
    nxNetShutdown();
    SDL_GameControllerClose(pad);
    SDL_Quit();
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

            int color = pixmap->pixels[h * pixmap->width + w];

            // draw cursor in here + redraw_background in poll_events
            int cx = screen_x - relative_x;
            int cy = screen_y - relative_y;
            if (cx >= 0 && cy >= 0 && cx < CURSOR_W && cy < CURSOR_H) {
                int i = (cy * CURSOR_W + cx) * 4;
                uint8_t a = cursor[i + 3];
                if (a > 0) {
                    uint8_t r = cursor[i];
                    uint8_t g = cursor[i + 1];
                    uint8_t b = cursor[i + 2];
                    color = (r << 16) | (g << 8) | b;
                }
            }

            rgbx[screen_y * SCREEN_FB_WIDTH + screen_x] = color;
        }
    }
}

void platform_poll_events(Client *c) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_CONTROLLERDEVICEADDED) {
            SDL_GameController *new_pad = SDL_GameControllerOpen(e.cdevice.which);
            if (!pad) {
                pad = new_pad;
            }
        } else if (e.type == SDL_CONTROLLERDEVICEREMOVED) {
            if (pad == SDL_GameControllerFromInstanceID(e.cdevice.which)) {
                pad = NULL;
            }
            SDL_GameControllerClose(SDL_GameControllerFromInstanceID(e.cdevice.which));
        } else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
            if (e.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                pad = (SDL_GameControllerFromInstanceID(e.cdevice.which));
            }
        }
    }

    SDL_GameControllerUpdate();

    static uint8_t prev_buttons[SDL_CONTROLLER_BUTTON_MAX];
    uint8_t current_buttons[SDL_CONTROLLER_BUTTON_MAX];

    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) {
        current_buttons[i] = SDL_GameControllerGetButton(pad, i);
    }

    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) {
        if (current_buttons[i] && !prev_buttons[i]) {
            switch (i) {
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    key_pressed(c->shell, K_LEFT, -1);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    key_pressed(c->shell, K_RIGHT, -1);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    key_pressed(c->shell, K_UP, -1);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    key_pressed(c->shell, K_DOWN, -1);
                    break;
                case SDL_CONTROLLER_BUTTON_X:
                    key_pressed(c->shell, K_CONTROL, -1);
                    break;
                case SDL_CONTROLLER_BUTTON_Y:
                    _Custom.showPerformance = !_Custom.showPerformance;
                    break;
                case SDL_CONTROLLER_BUTTON_BACK:
                    if (c->ingame) client_logout(c);
                    break;
                case SDL_CONTROLLER_BUTTON_START:
                    if (!c->ingame) client_login(c, c->username, c->password, false);
                    break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                    // viewport start
                    screen_offset_x = -8;
                    screen_offset_y = -11;
                    c->redraw_background = true;
                    break;
                case SDL_CONTROLLER_BUTTON_A:
                case SDL_CONTROLLER_BUTTON_B:
                    c->shell->idle_cycles = 0;
                    c->shell->mouse_click_x = cursor_x;
                    c->shell->mouse_click_y = cursor_y;

                    if (i == SDL_CONTROLLER_BUTTON_A) {
                        c->shell->mouse_click_button = 2;
                        c->shell->mouse_button = 2;
                    } else {
                        c->shell->mouse_click_button = 1;
                        c->shell->mouse_button = 1;
                    }

                    if (_InputTracking.enabled) {
                        inputtracking_mouse_pressed(&_InputTracking, cursor_x, cursor_y, i == SDL_CONTROLLER_BUTTON_A ? 1 : 0);
                    }
                    break;
            }
        } else if (!current_buttons[i] && prev_buttons[i]) {
            switch (i) {
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    key_released(c->shell, K_LEFT, -1);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    key_released(c->shell, K_RIGHT, -1);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    key_released(c->shell, K_UP, -1);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    key_released(c->shell, K_DOWN, -1);
                    break;
                case SDL_CONTROLLER_BUTTON_X:
                    key_released(c->shell, K_CONTROL, -1);
                    break;
                case SDL_CONTROLLER_BUTTON_A:
                case SDL_CONTROLLER_BUTTON_B:
                    c->shell->idle_cycles = 0;
                    c->shell->mouse_button = 0;

                    if (_InputTracking.enabled) {
                        inputtracking_mouse_released(&_InputTracking, i == SDL_CONTROLLER_BUTTON_A ? 1 : 0);
                    }
                    break;
            }
        }
    }

    memcpy(prev_buttons, current_buttons, sizeof(prev_buttons));

    // TODO might not work on real hw and unused: SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY
    if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX) != 0 || SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY) != 0 || SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY) != -1) {
        c->redraw_background = true;

        // TODO allow changing cursor sensitivity, don't move cursor while panning, this still moves cursor if unable to pan though
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) {
            screen_offset_x = MAX(SCREEN_FB_WIDTH - SCREEN_WIDTH, MIN(screen_offset_x - SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX) / CURSOR_SENSITIVITY, 0));
            screen_offset_y = MAX(SCREEN_FB_HEIGHT - SCREEN_HEIGHT, MIN(screen_offset_y - SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY) / CURSOR_SENSITIVITY, 0));
            // c->redraw_background = true;
        }

        cursor_x = MAX(0, MIN(cursor_x + SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX) / CURSOR_SENSITIVITY, SCREEN_WIDTH - 1));
        cursor_y = MAX(0, MIN(cursor_y + SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY) / CURSOR_SENSITIVITY, SCREEN_HEIGHT - 1));

        int x = cursor_x;
        int y = cursor_y;

        c->shell->idle_cycles = 0;
        c->shell->mouse_x = x;
        c->shell->mouse_y = y;

        if (_InputTracking.enabled) {
            inputtracking_mouse_moved(&_InputTracking, x, y);
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
    return GetTickCount();
}
void rs2_sleep(int ms) {
    Sleep(ms);
}
#endif
