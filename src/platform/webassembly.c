#if defined(__wasm) && !defined(__EMSCRIPTEN__)
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <js/glue.h>

#include "../clientstream.h"
#include "../defines.h"
#include "../pixmap.h"
#include "../platform.h"

// TODO missing clang compiler-rt stub
int __unordtf2(int64_t a, int64_t b, int64_t c, int64_t d);
int __unordtf2(int64_t a, int64_t b, int64_t c, int64_t d) {
    (void)a, (void)b, (void)c, (void)d;
    return 0;
}

uint32_t canvas[SCREEN_WIDTH * SCREEN_HEIGHT];

bool clientstream_init(void) { return true; }
ClientStream *clientstream_new(void) { return 0; }
ClientStream *clientstream_opensocket(int port) {
    (void)port;
    return NULL;
}
void clientstream_close(ClientStream *stream) { (void)stream; }
int clientstream_available(ClientStream *stream, int length) {
    (void)stream, (void)length;
    return 0;
}
int clientstream_read_byte(ClientStream *stream) {
    (void)stream;
    return 0;
}
int clientstream_read_bytes(ClientStream *stream, int8_t *dst, int off, int len) {
    (void)stream, (void)dst, (void)off, (void)len;
    return 0;
}
int clientstream_write(ClientStream *stream, const int8_t *src, int len, int off) {
    (void)stream, (void)src, (void)len, (void)off;
    return 0;
}
const char *dnslookup(const char *hostname) {
    (void)hostname;
    return NULL;
}

bool platform_init(void) { return true; }
void platform_new(int width, int height) {
    JS_createCanvas(width, height, "Jagex");
}
void platform_free(void) {}
void platform_set_wave_volume(int wavevol) {
    (void)wavevol;
}
void platform_play_wave(int8_t *src, int length) {
    (void)src, (void)length;
}
void platform_set_midi_volume(float midivol) {
    (void)midivol;
}
void platform_set_jingle(int8_t *src, int len) {
    (void)src, (void)len;
}
void platform_set_midi(const char *name, int crc, int len) {
    (void)name, (void)crc, (void)len;
}
void platform_stop_midi(void) {}
void set_pixels(PixMap *pixmap, int x, int y) {
    for (int h = 0; h < pixmap->height; h++) {
        for (int w = 0; w < pixmap->width; w++) {
            uint32_t pixel = pixmap->pixels[h * pixmap->width + w];
            pixel = ((pixel & 0xff0000) >> 16) | (pixel & 0x00ff00) | ((pixel & 0x0000ff) << 16) | 0xff000000;
            canvas[(y + h) * SCREEN_FB_WIDTH + (x + w)] = pixel;
        }
    }

    JS_setPixelsAlpha(canvas);
    platform_update_surface();
}
void platform_poll_events(Client *c) { (void)c; }
void platform_draw_string(const char *str, int x, int y, int color, bool bold, int size) {
    (void)bold, (void)size;

    char buf[8];
    snprintf(buf, sizeof(buf), "#%06x", color);
    JS_fillStyle(buf);
    JS_fillText(str, x, y);
}
void platform_blit_surface(int x, int y, int w, int h, Surface *surface) {
    (void)x, (void)y, (void)w, (void)w, (void)h, (void)surface;
}
void platform_update_surface(void) {
    JS_requestAnimationFrame();
}
void platform_draw_rect(int x, int y, int w, int h, int color) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%06x", color);
    JS_strokeStyle(buf);
    JS_strokeRect(x, y, w, h);
}
void platform_fill_rect(int x, int y, int w, int h, int color) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%06x", color);
    JS_fillStyle(buf);
    JS_fillRect(x, y, w, h);
}
uint64_t rs2_now(void) {
    return JS_performanceNow();
}
void rs2_sleep(int ms) {
    JS_setTimeout(ms);
}
#endif
