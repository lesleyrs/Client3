#if defined(__wasm) && !defined(__EMSCRIPTEN__)
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <js/glue.h>
#include <js/dom_pk_codes.h>

#include "../clientstream.h"
#include "../defines.h"
#include "../pixmap.h"
#include "../platform.h"
#include "../client.h"
#include "../gameshell.h"
#include "../inputtracking.h"

extern ClientData _Client;
extern InputTracking _InputTracking;

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
    JS_createCanvas(width, height);
    JS_setTitle("Jagex");
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

static bool onmousemove(void *user_data, int x, int y) {
    Client *c = (Client *)user_data;
    c->shell->idle_cycles = 0;
    c->shell->mouse_x = x;
    c->shell->mouse_y = y;

    if (_InputTracking.enabled) {
        inputtracking_mouse_moved(&_InputTracking, x, y);
    }
    return 0;
}

static bool onmouse(void *user_data, bool pressed, int button) {
    Client *c = (Client *)user_data;
    c->shell->idle_cycles = 0;

    if (pressed) {
        c->shell->mouse_click_x = c->shell->mouse_x;
        c->shell->mouse_click_y = c->shell->mouse_y;

        if (button == 2) {
            c->shell->mouse_click_button = 2;
            c->shell->mouse_button = 2;
        } else {
            c->shell->mouse_click_button = 1;
            c->shell->mouse_button = 1;
        }

        if (_InputTracking.enabled) {
            inputtracking_mouse_pressed(&_InputTracking, c->shell->mouse_x, c->shell->mouse_y, button == 2 ? 1 : 0);
        }
    } else {
        c->shell->mouse_button = 0;

        if (_InputTracking.enabled) {
            inputtracking_mouse_released(&_InputTracking, (button == 2) != 0 ? 1 : 0);
        }
    }
    return 0;
}

static int convert_pk(GameShell *shell, int ch, int code, int modifiers, bool down) {
    shell->idle_cycles = 0;

    // java ctrl key lowers char value
    if (modifiers & KMOD_CTRL) {
        if ((ch >= 'A' && ch <= ']') || ch == '_') {
            ch -= 'A' - 1;
        } else if (ch >= 'a' && ch <= 'z') {
            ch -= 'a' - 1;
        }
    }

    if (ch < 30) {
        ch = 0;
    }

    switch (code) {
    case DOM_PK_ARROW_LEFT:
        ch = 1;
        break;
    case DOM_PK_ARROW_RIGHT:
        ch = 2;
        break;
    case DOM_PK_ARROW_UP:
        ch = 3;
        break;
    case DOM_PK_ARROW_DOWN:
        ch = 4;
        break;
    case DOM_PK_CONTROL_LEFT:
    case DOM_PK_CONTROL_RIGHT:
        ch = 5;
        break;
    case DOM_PK_SHIFT_LEFT:
    case DOM_PK_SHIFT_RIGHT:
        ch = 6; // (custom)
        break;
    case DOM_PK_ALT_LEFT:
    case DOM_PK_ALT_RIGHT:
        ch = 7; // (custom)
        break;
    case DOM_PK_BACKSPACE:
        ch = 8;
        break;
    case DOM_PK_DELETE:
        ch = 8;
        break;
    case DOM_PK_TAB:
        ch = 9;
        break;
    case DOM_PK_ENTER:
        ch = 10;
        break;
    case DOM_PK_HOME:
        ch = 1000;
        break;
    case DOM_PK_END:
        ch = 1001;
        break;
    case DOM_PK_PAGE_UP:
        ch = 1002;
        break;
    case DOM_PK_PAGE_DOWN:
        ch = 1003;
        break;
    }

    // no f11/f12 (not right after f10 in mapping, but also not used)
    if (code >= DOM_PK_F1 && code <= DOM_PK_F10) {
        ch = code + 1008 - DOM_PK_F1;
    }

    if (ch > 0 && ch < 128) {
        shell->action_key[ch] = down;
    }

    if (down) {
        if (ch > 4) {
            shell->key_queue[shell->key_queue_write_pos] = ch;
            shell->key_queue_write_pos = shell->key_queue_write_pos + 1 & 0x7f;
        }
    }

    // allow default browser action
    if (code == DOM_PK_F5 || code == DOM_PK_F11 || code == DOM_PK_F12) {
        return 0;
    }
    // return 1 for preventDefault()
    return 1;
}

static bool onkey(void *user_data, bool pressed, int key, int code, int modifiers) {
    Client *c = (Client *)user_data;

    int rc = convert_pk(c->shell, key, code, modifiers, pressed);

    if (_InputTracking.enabled) {
        if (pressed) {
            inputtracking_key_pressed(&_InputTracking, key);
        } else {
            inputtracking_key_released(&_InputTracking, key);
        }
    }
    return rc;
}

void platform_poll_events(Client *c) {
    static bool init;
    if (!init) {
        JS_addMouseMoveEventListener(c, onmousemove);
        JS_addMouseEventListener(c, onmouse);

        JS_addKeyEventListener(c, onkey);
        init = true;
    }
}

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
