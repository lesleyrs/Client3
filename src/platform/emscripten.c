#if defined(__EMSCRIPTEN__) && (!defined(SDL) || SDL == 0)
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/key_codes.h>

#include <malloc.h>

#include "../client.h"
#include "../custom.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../platform.h"

extern ClientData _Client;
extern InputTracking _InputTracking;
extern Custom _Custom;

EM_JS(void, draw_string_js, (const char *str, int x, int y, int color, bool bold, int size), {
    var canvas = document.getElementById('canvas');
    var ctx = canvas.getContext('2d');
    var weight = bold ? 'bold ' : 'normal ';
    ctx.font = weight + size + 'px helvetica, sans-serif';
    let hexColor = '#' + ('000000' + color.toString(16)).slice(-6);
    ctx.fillStyle = hexColor;
    if (x == Math.floor(canvas.width / 2)) {
        ctx.textAlign = 'center';
    } else {
        ctx.textAlign = 'left';
    }
    ctx.fillText(UTF8ToString(str), x, y);
})

EM_JS(void, draw_rect_js, (int x, int y, int w, int h, int color), {
    var ctx = document.getElementById('canvas').getContext('2d');
    let hexColor = '#' + ('000000' + color.toString(16)).slice(-6);
    ctx.fillStyle = hexColor;
    ctx.rect(x, y, w, h);
})

EM_JS(void, fill_rect_js, (int x, int y, int w, int h, int color), {
    var ctx = document.getElementById('canvas').getContext('2d');
    let hexColor = '#' + ('000000' + color.toString(16)).slice(-6);
    ctx.fillStyle = hexColor;
    ctx.fillRect(x, y, w, h);
})

EM_JS(float, frame_insets_left_js, (void), {
    var canvas = document.getElementById('canvas');
    var rect = canvas.getBoundingClientRect();
    return rect.left;
})

EM_JS(float, frame_insets_top_js, (void), {
    var canvas = document.getElementById('canvas');
    var rect = canvas.getBoundingClientRect();
    return rect.top;
})

EM_JS(void, set_pixels_js, (int x, int y, int width, int height, int *pixels), {
    var imageData = new ImageData(width, height);
    var data = new DataView(imageData.data.buffer);

    var pixelArray = new Uint32Array(HEAP32.buffer, pixels, width * height);

    for (var h = 0; h < height; h++) {
        for (var w = 0; w < width; w++) {
            var idx = h * width + w;
            var pixel = pixelArray[idx];

            pixel = ((pixel & 0xff0000) >> 16) | (pixel & 0x00ff00) | ((pixel & 0x0000ff) << 16) | 0xff000000;
            data.setUint32(idx * 4, pixel, true);
        }
    }

    var ctx = document.getElementById('canvas').getContext('2d');
    ctx.putImageData(imageData, x, y);
})

bool platform_init(void) {
    return true;
}

void platform_new(int width, int height) {
    emscripten_set_canvas_element_size("#canvas", width, height);
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
    set_pixels_js(x, y, pixmap->width, pixmap->height, pixmap->pixels);
}

static bool onmousemove(int event_type, const EmscriptenMouseEvent *e, void *user_data) {
    (void)event_type;
    Client *c = (Client *)user_data;
    int x = e->clientX - frame_insets_left_js();
    int y = e->clientY - frame_insets_top_js();

    c->shell->idle_cycles = 0;
    c->shell->mouse_x = x;
    c->shell->mouse_y = y;

    if (_InputTracking.enabled) {
        inputtracking_mouse_moved(&_InputTracking, x, y);
    }
    return 0;
}

static bool onmousedown(int event_type, const EmscriptenMouseEvent *e, void *user_data) {
    (void)event_type;
    Client *c = (Client *)user_data;
    int x = e->clientX - frame_insets_left_js();
    int y = e->clientY - frame_insets_top_js();

    c->shell->idle_cycles = 0;
    c->shell->mouse_click_x = x;
    c->shell->mouse_click_y = y;

    if (e->button == 2) {
        c->shell->mouse_click_button = 2;
        c->shell->mouse_button = 2;
    } else {
        c->shell->mouse_click_button = 1;
        c->shell->mouse_button = 1;
    }

    if (_InputTracking.enabled) {
        inputtracking_mouse_pressed(&_InputTracking, x, y, e->button == 2 ? 1 : 0);
    }
    return 0;
}

static bool onmouseup(int event_type, const EmscriptenMouseEvent *e, void *user_data) {
    (void)event_type;
    Client *c = (Client *)user_data;
    c->shell->idle_cycles = 0;
    c->shell->mouse_button = 0;

    if (_InputTracking.enabled) {
        inputtracking_mouse_released(&_InputTracking, (e->button == 2) != 0 ? 1 : 0);
    }
    return 0;
}

static bool onkeydown(int event_type, const EmscriptenKeyboardEvent *e, void *user_data) {
    (void)event_type;
    Client *c = (Client *)user_data;
    switch (e->keyCode) {
    case K_UP:
    case K_DOWN:
    case K_LEFT:
    case K_RIGHT:
        key_pressed(c->shell, e->keyCode, -1);
        break;
    }
    rs2_log("down %d\n", e->keyCode);
    return 0;
}

static bool onkeyup(int event_type, const EmscriptenKeyboardEvent *e, void *user_data) {
    (void)event_type;
    Client *c = (Client *)user_data;
    switch (e->keyCode) {
    case K_UP:
    case K_DOWN:
    case K_LEFT:
    case K_RIGHT:
        key_released(c->shell, e->keyCode, -1);
        break;
    }
    rs2_log("up %d\n", e->keyCode);
    return 0;
}

static bool init;
void platform_poll_events(Client *c) {
    if (!init) {
        emscripten_set_mousemove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, onmousemove);
        emscripten_set_mousedown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, onmousedown);
        emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, onmouseup);

        emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, onkeydown);
        emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, onkeyup);

        // emscripten_set_mouseenter_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, onmousefocus);
        // emscripten_set_mouseleave_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, onmouseleave);

        // emscripten_set_wheel_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, Emscripten_HandleWheel);

        // emscripten_set_focus_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, Emscripten_HandleFocus);
        // emscripten_set_blur_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, Emscripten_HandleFocus);

        // emscripten_set_touchstart_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, Emscripten_HandleTouch);
        // emscripten_set_touchend_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, Emscripten_HandleTouch);
        // emscripten_set_touchmove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, Emscripten_HandleTouch);
        // emscripten_set_touchcancel_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, Emscripten_HandleTouch);

        // emscripten_set_fullscreenchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, c, false, Emscripten_HandleFullscreenChange);

        // emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, Emscripten_HandleResize);

        // emscripten_set_visibilitychange_callback(c, false, Emscripten_HandleVisibilityChange);

        // emscripten_set_beforeunload_callback(c, Emscripten_HandleBeforeUnload);

        init = true;
    }
}
// TODO remove blit_surface, unused for web
void platform_blit_surface(int x, int y, int w, int h, Surface *surface) {
    (void)x, (void)y, (void)w, (void)h, (void)surface;
}
void platform_draw_string(const char *str, int x, int y, int color, bool bold, int size) {
    draw_string_js(str, x, y, color, bold, size);
}
void platform_update_surface(void) {
    delay_ticks(0); // return a slice of time to the main loop so it can update the progress bar
}
void platform_draw_rect(int x, int y, int w, int h, int color) {
    draw_rect_js(x, y, w, h, color);
}
void platform_fill_rect(int x, int y, int w, int h, int color) {
    fill_rect_js(x, y, w, h, color);
}
uint64_t get_ticks(void) {
    return emscripten_get_now();
}
void delay_ticks(int ticks) {
    emscripten_sleep(ticks);
}
#endif
