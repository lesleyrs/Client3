#if defined(__vita__) && (!defined(SDL) || SDL == 0)
#include <malloc.h>

#include <psp2/display.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>

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

bool platform_init(void) {
    return true;
}

void platform_new(int width, int height) {
    (void)width, (void)height;
    mutex = sceKernelCreateMutex("fb_mutex", 0, 0, NULL);
    displayblock = sceKernelAllocMemBlock("display", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, SCREEN_FB_SIZE, NULL);
    if (displayblock < 0)
        return;
    sceKernelGetMemBlockBase(displayblock, (void **)&base);
    SceDisplayFrameBuf frame = {sizeof(frame), base, (SCREEN_FB_WIDTH), 0, SCREEN_FB_WIDTH, SCREEN_FB_HEIGHT};
    sceDisplaySetFrameBuf(&frame, SCE_DISPLAY_SETBUF_NEXTFRAME);
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
Surface *platform_create_surface(int *pixels, int width, int height, int alpha) {
    Surface *surface = calloc(1, sizeof(Surface));
    surface->pixels = pixels;
    return surface;
}
void platform_free_surface(Surface *surface) {
    free(surface);
}
void set_pixels(PixMap *pixmap, int x, int y) {
    sceKernelLockMutex(mutex, 1, NULL);
    for (int h = 0; h < pixmap->height; h++) {
        for (int w = 0; w < pixmap->width; w++) {
            uint32_t pixel = pixmap->pixels[h * pixmap->width + w];
            pixel = ((pixel & 0xff0000) >> 16) | (pixel & 0x00ff00) | ((pixel & 0x0000ff) << 16);
            ((uint32_t *)base)[(y + h) * SCREEN_FB_WIDTH + (x + w)] = pixel;
        }
    }
    sceKernelUnlockMutex(mutex, 1);
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
    return sceKernelGetSystemTimeWide() / 1000;
}
void delay_ticks(int ticks) {
    sceKernelDelayThreadCB(ticks * 1000);
}
#endif
