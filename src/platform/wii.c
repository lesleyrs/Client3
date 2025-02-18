#ifdef __WII__
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <asndlib.h>
#include <gccore.h>
#include <ogc/usbmouse.h>
#include <wiikeyboard/keyboard.h>
#include <wiiuse/wpad.h>

#include <ogc/lwp_watchdog.h>
#include <ogcsys.h>

#include "../gameshell.h"
#include "../pixmap.h"
#include "../platform.h"

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

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

void platform_new(GameShell *shell, int width, int height) {
    (void)shell, (void)width, (void)height;
    // Initialise the video system
    VIDEO_Init();

    // This function initialises the attached controllers
    WPAD_Init();

    // Obtain the preferred video mode from the system
    // This will correspond to the settings in the Wii menu
    rmode = VIDEO_GetPreferredMode(NULL);

    // Allocate memory for the display in the uncached region
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    // Initialise the console, required for printf
    console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
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

    // WPAD_SetDataFormat(0, WPAD_FMT_BTNS_ACC_IR);
    // WPAD_SetVRes(0, rmode->fbWidth, rmode->xfbHeight);

    KEYBOARD_Init(NULL);
    MOUSE_Init();
    initfs();
    // The console understands VT terminal escape codes
    // This positions the cursor on row 2, column 0
    // we can use variables for this with format codes too
    // e.g. printf ("\x1b[%d;%dH", row, column );
    printf("\x1b[2;0H");

    printf("Hello World!\n");

    // while(1) {

    // Call WPAD_ScanPads each loop, this reads the latest controller states
    // WPAD_ScanPads();

    // WPAD_ButtonsDown tells us which buttons were pressed in this loop
    // this is a "one shot" state which will not fire again until the button has been released
    // u32 pressed = WPAD_ButtonsDown(0);

    // We return to the launcher application via exit
    // if ( pressed & WPAD_BUTTON_HOME ) exit(0);

    // Wait for the next frame
    // VIDEO_WaitVSync();
    // }
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
        if (y + row >= 480) break;

        for (int col = 0; col < pixmap->width; col += 2) {
            if (x + col >= 640) break;

            int src_offset = row * pixmap->width + col;
            int dest_offset = (y + row) * (rmode->fbWidth / 2) + (x + col) / 2;

            int pixel1 = pixmap->pixels[src_offset];
            uint8_t r1 = (pixel1 >> 16) & 0xff;
            uint8_t g1 = (pixel1 >> 8) & 0xff;
            uint8_t b1 = pixel1 & 0xff;

            int pixel2 = pixmap->pixels[src_offset+1];
            uint8_t r2 = (pixel2 >> 16) & 0xff;
            uint8_t g2 = (pixel2 >> 8) & 0xff;
            uint8_t b2 = pixel2 & 0xff;

            ((uint32_t *)xfb)[dest_offset] = rgb2yuv(r1, g1, b1, r2, g2, b2);
        }
    }

    // VIDEO_Flush();
    // VIDEO_WaitVSync();
}
void platform_get_keycodes(Keysym *keysym, int *code, char *ch) {
}
void platform_poll_events(Client *c) {
}
void platform_update_surface(GameShell *shell) {
}
void platform_fill_rect(GameShell *shell, int x, int y, int w, int h, int color) {
}
void platform_blit_surface(GameShell *shell, int x, int y, int w, int h, Surface *surface) {
}
int64_t get_ticks(void) {
    uint64_t ticks = gettime();
    return (int)(ticks / TB_TIMER_CLOCK);
}
void delay_ticks(int ticks) {
    int end = get_ticks() + ticks;

    while (get_ticks() != end)
        ;
}
void rs2_log(const char *format, ...) {
    va_list args;
    va_start(args, format);

    vprintf(format, args);

    va_end(args);
}

void rs2_error(const char *format, ...) {
    va_list args;
    va_start(args, format);

    vfprintf(stderr, format, args);

    va_end(args);
}
#endif
