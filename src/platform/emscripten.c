#if defined(__EMSCRIPTEN__) && (!defined(SDL) || SDL == 0)
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/key_codes.h>

#include <errno.h>
#include <malloc.h>

#include "../client.h"
#include "../custom.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../platform.h"

extern ClientData _Client;
extern InputTracking _InputTracking;
extern Custom _Custom;

#include "../thirdparty/bzip.h"
#define TSF_IMPLEMENTATION
#include "../thirdparty/tsf.h"

#define TML_IMPLEMENTATION
#include "../thirdparty/tml.h"

static tml_message *TinyMidiLoader;
// Holds the global instance pointer
static tsf *g_TinySoundFont;
// Holds global MIDI playback state
static double g_Msec;              // current playback time
static tml_message *g_MidiMessage; // next message to be played

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
    ctx.strokeStyle = hexColor;
    ctx.strokeRect(x, y, w, h);
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

EM_JS(void, set_wave_volume_js, (int wavevol), {
    setWaveVolume(wavevol);
})

EM_JS(void, play_wave_js, (int8_t * src, int length), {
    playWave(HEAP8.subarray(src, src + length));
})

static void midi_callback(void *data, uint8_t *stream, int len) {
    (void)data;
// Number of samples to process
#if SDL == 1
    int SampleBlock, SampleCount = (len / (2 * sizeof(short))); // 2 output channels
    for (SampleBlock = TSF_RENDER_EFFECTSAMPLEBLOCK; SampleCount; SampleCount -= SampleBlock, stream += (SampleBlock * (2 * sizeof(short)))) {
#else
    int SampleBlock, SampleCount = (len / (2 * sizeof(float))); // 2 output channels
    for (SampleBlock = TSF_RENDER_EFFECTSAMPLEBLOCK; SampleCount; SampleCount -= SampleBlock, stream += (SampleBlock * (2 * sizeof(float)))) {
#endif
        // We progress the MIDI playback and then process TSF_RENDER_EFFECTSAMPLEBLOCK samples at once
        if (SampleBlock > SampleCount)
            SampleBlock = SampleCount;

        // Loop through all MIDI messages which need to be played up until the current playback time
        for (g_Msec += SampleBlock * (1000.0 / 44100.0); g_MidiMessage && g_Msec >= g_MidiMessage->time; g_MidiMessage = g_MidiMessage->next) {
            switch (g_MidiMessage->type) {
            case TML_PROGRAM_CHANGE: // channel program (preset) change (special handling for 10th MIDI channel with drums)
                tsf_channel_set_presetnumber(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->program, (g_MidiMessage->channel == 9));
                break;
            case TML_NOTE_ON: // play a note
                tsf_channel_note_on(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->key, g_MidiMessage->velocity / 127.0f);
                break;
            case TML_NOTE_OFF: // stop a note
                tsf_channel_note_off(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->key);
                break;
            case TML_PITCH_BEND: // pitch wheel modification
                tsf_channel_set_pitchwheel(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->pitch_bend);
                break;
            case TML_CONTROL_CHANGE: // MIDI controller messages
                tsf_channel_midi_control(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->control, g_MidiMessage->control_value);
                break;
            }
        }

// Render the block of audio samples in float format
#if SDL == 1
        tsf_render_short(g_TinySoundFont, (int16_t *)stream, SampleBlock, 0);
#else
        tsf_render_float(g_TinySoundFont, (float *)stream, SampleBlock, 0);
#endif
    }
}

bool platform_init(void) {
    return true;
}

#include <SDL.h>
void platform_new(int width, int height) {
    emscripten_set_canvas_element_size("#canvas", width, height);

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        rs2_error("SDL_Init failed: %s\n", SDL_GetError());
        return;
    }

    SDL_AudioSpec midiSpec;
    midiSpec.freq = 44100;
#if SDL == 1
    midiSpec.format = AUDIO_S16SYS;
#else
    midiSpec.format = AUDIO_F32;
#endif
    midiSpec.channels = 2;
    midiSpec.samples = 4096;
    midiSpec.callback = midi_callback;

    void *buffer = NULL;
    int size = 0;
    int error = 0;
    emscripten_wget_data("SCC1_Florestan.sf2", &buffer, &size, &error);
    if (error) {
        rs2_error("Error downloading SoundFont: %d\n", error);
    }

    g_TinySoundFont = tsf_load_memory(buffer, size);
    if (!g_TinySoundFont) {
        rs2_error("Could not load SoundFont\n");
    } else {
        // Set the SoundFont rendering output mode
        tsf_set_output(g_TinySoundFont, TSF_STEREO_INTERLEAVED, midiSpec.freq, 0.0f);

        if (SDL_OpenAudio(&midiSpec, NULL) < 0) {
            rs2_error("Could not open the audio hardware or the desired audio output format\n");
        }
        SDL_PauseAudio(0);
    }
}

void platform_free(void) {
}
void platform_set_wave_volume(int wavevol) {
    set_wave_volume_js(wavevol);
}
void platform_play_wave(int8_t *src, int length) {
    play_wave_js(src, length);
}
void platform_set_midi_volume(float midivol) {
    if (_Client.lowmem) {
        return;
    }
    if (SDL_GetAudioStatus() != SDL_AUDIO_STOPPED) {
        tsf_set_volume(g_TinySoundFont, midivol);
    }
}
void platform_set_jingle(int8_t *src, int len) {
    // tml_free(TinyMidiLoader);
    TinyMidiLoader = tml_load_memory(src, len);
    platform_stop_midi();
    g_MidiMessage = TinyMidiLoader;
    free(src);
}
void platform_set_midi(const char *name, int crc, int len) {
    char filename[PATH_MAX];
    void *data = NULL;
    int data_len = 0;
    snprintf(filename, sizeof(filename), "%s_%d.mid", name, crc);
    data = client_open_url(filename, &data_len);
    if (data && crc != 12345678) {
        int data_crc = rs_crc32(data, len);
        if (data_crc != crc) {
            free(data);
            data = NULL;
        }
    }

    Packet *packet = packet_new(data, 4);
    const int uncompressed_length = g4(packet);
    int8_t *uncompressed = malloc(uncompressed_length);
    bzip_decompress(uncompressed, data, data_len - 4, 4);
    // tml_free(TinyMidiLoader);
    TinyMidiLoader = tml_load_memory(uncompressed, uncompressed_length);
    platform_stop_midi();
    g_MidiMessage = TinyMidiLoader;
    packet_free(packet);
    free(uncompressed);
}
void platform_stop_midi(void) {
    if (_Client.lowmem) {
        return;
    }
    if (SDL_GetAudioStatus() != SDL_AUDIO_STOPPED) {
        g_MidiMessage = NULL;
        g_Msec = 0;
        tsf_reset(g_TinySoundFont);
        // Initialize preset on special 10th MIDI channel to use percussion sound bank (128) if available
        tsf_channel_set_bank_preset(g_TinySoundFont, 9, 128, 0);
    }
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

static void platform_get_keycodes(const EmscriptenKeyboardEvent *e, int *code, unsigned char *ch) {
    switch (e->keyCode) {
    case K_UP:
    case K_DOWN:
    case K_LEFT:
    case K_RIGHT:
    case K_CONTROL:
    case K_PAGE_UP:
    case K_PAGE_DOWN:
    case K_END:
    case K_HOME:
    case K_F1:
    case K_F2:
    case K_F3:
    case K_F4:
    case K_F5:
    case K_F6:
    case K_F7:
    case K_F8:
    case K_F9:
    case K_F10:
    case K_F11:
    case K_F12:
    case K_ESCAPE:
    case K_ENTER:
    default:
        *code = e->keyCode;
        *ch = e->keyCode;

        if (!e->shiftKey) {
            if (*ch >= 'A' && *ch <= 'Z') {
                *ch += 32;
            }
            switch (e->keyCode) {
            case 173:
                *ch = '-';
                break;
            case 188:
                *ch = ',';
                break;
            case 190:
                *ch = '.';
                break;
            case 191:
                *ch = '/';
                break;
            case 192: // '`'
                *ch = -1;
                break;
            case 219:
                *ch = '[';
                break;
            case 220:
                *ch = '\\';
                break;
            case 221:
                *ch = ']';
                break;
            case 222:
                *ch = '\'';
            }
        } else {
            switch (*ch) {
            case '1':
                *ch = '!';
                break;
            case '2':
                *ch = '@';
                break;
            case '3':
                *ch = '#';
                break;
            case '4':
                *ch = '$';
                break;
            case '5':
                *ch = '%';
                break;
            case '6':
                *ch = '^';
                break;
            case '7':
                *ch = '&';
                break;
            case '8':
                *ch = '*';
                break;
            case '9':
                *ch = '(';
                break;
            case '0':
                *ch = ')';
                break;
            case ';':
                *ch = ':';
                break;
            case '=':
                *ch = '+';
                break;
            case 173:
                *ch = '_';
                break;
            case 188:
                *ch = '<';
                break;
            case 190:
                *ch = '>';
                break;
            case 191:
                *ch = '?';
                break;
            case 192:
                *ch = '~';
                break;
            case 219:
                *ch = '{';
                break;
            case 220:
                *ch = '|';
                break;
            case 221:
                *ch = '}';
                break;
            case 222:
                *ch = '"';
            }
        }
    }
}

static bool onkeydown(int event_type, const EmscriptenKeyboardEvent *e, void *user_data) {
    (void)event_type;
    Client *c = (Client *)user_data;

    int code = -1;
    unsigned char ch = -1;
    platform_get_keycodes(e, &code, &ch);
    key_pressed(c->shell, code, ch);
    // TODO rm
    // rs2_log("down %d\n", e->keyCode);
    return 0;
}

static bool onkeyup(int event_type, const EmscriptenKeyboardEvent *e, void *user_data) {
    (void)event_type;
    Client *c = (Client *)user_data;

    int code = -1;
    unsigned char ch = -1;
    platform_get_keycodes(e, &code, &ch);
    key_released(c->shell, code, ch);
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
