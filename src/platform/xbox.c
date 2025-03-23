#if defined(NXDK)
#include <hal/debug.h>
#include <hal/xbox.h>
#include <nxdk/mount.h>
#include <nxdk/net.h>
#include <windows.h>

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

    return true;
}

void platform_new(int width, int height) {
    (void)width, (void)height;
}

void platform_free(void) {
    nxNetShutdown();
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
            rgbx[screen_y * SCREEN_FB_WIDTH + screen_x] = pixmap->pixels[h * pixmap->width + w];
        }
    }
}

void platform_poll_events(Client *c) {
}
void platform_update_surface(void) {
}
void platform_fill_rect(int x, int y, int w, int h, int color) {
}
void platform_blit_surface(int x, int y, int w, int h, Surface *surface) {
}
uint64_t get_ticks(void) {
    return GetTickCount();
}
void delay_ticks(int ticks) {
    Sleep(ticks);
}
#endif
