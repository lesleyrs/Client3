#ifdef __EMSCRIPTEN__
#include <SDL3/SDL.h>
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

EM_JS(void, set_font_js, (const char* name, bool bold, int size), {
    const canvas = document.getElementById('canvas');
    const ctx = canvas.getContext('2d');
    const weight = bold ? 'bold ' : 'normal ';
    ctx.font = weight + size + 'px Helvetica, sans-serif';
})

EM_JS(void, set_color_js, (int color), {
    const canvas = document.getElementById('canvas');
    const ctx = canvas.getContext('2d');
    const hexColor = '#' + ('000000' + color.toString(16)).slice(-6);
    ctx.fillStyle = hexColor;
    ctx.strokeStyle = hexColor;
})

EM_JS(int, string_width_js, (const char* str), {
    const canvas = document.getElementById('canvas');
    const ctx = canvas.getContext('2d');
    return ctx.measureText(UTF8ToString(str)).width;
})

EM_JS(void, draw_string_js, (const char *str, int x, int y), {
    const canvas = document.getElementById('canvas');
    const ctx = canvas.getContext('2d');
    ctx.fillText(UTF8ToString(str), x, y);
})

EM_JS(void, draw_rect_js, (int x, int y, int w, int h), {
    const ctx = document.getElementById('canvas').getContext('2d');
    ctx.strokeRect(x, y, w, h);
})

EM_JS(void, fill_rect_js, (int x, int y, int w, int h), {
    const ctx = document.getElementById('canvas').getContext('2d');
    ctx.fillRect(x, y, w, h);
})

EM_JS(float, frame_insets_left_js, (void), {
    const canvas = document.getElementById('canvas');
    const rect = canvas.getBoundingClientRect();
    return rect.left;
})

EM_JS(float, frame_insets_top_js, (void), {
    const canvas = document.getElementById('canvas');
    const rect = canvas.getBoundingClientRect();
    return rect.top;
})

EM_JS(void, set_pixels_js, (int x, int y, int width, int height, int *pixels), {
    const imageData = new ImageData(width, height);
    const data = new DataView(imageData.data.buffer);

    const pixelArray = new Uint32Array(HEAP32.buffer, pixels, width * height);

    for (let h = 0; h < height; h++) {
        for (let w = 0; w < width; w++) {
            const idx = h * width + w;
            let pixel = pixelArray[idx];

            pixel = ((pixel & 0xff0000) >> 16) | (pixel & 0x00ff00) | ((pixel & 0x0000ff) << 16) | 0xff000000;
            data.setUint32(idx * 4, pixel, true);
        }
    }

    const ctx = document.getElementById('canvas').getContext('2d');
    ctx.putImageData(imageData, x, y);
})

static float *midi_buffer;
static SDL_AudioStream *midi_stream;
static SDL_AudioStream *wave_stream = NULL;
static int g_wavevol = 128;
void platform_play_wave(int8_t *src, int length) {
    SDL_AudioSpec wave_spec = {0};
    uint8_t *wave_buffer = NULL;
    uint32_t wave_length = 0;
    SDL_IOStream *sdl_iostream = SDL_IOFromMem(src, length);
    if (!SDL_LoadWAV_IO(sdl_iostream, true, &wave_spec, &wave_buffer, &wave_length)) {
        rs2_error("SDL3: LoadWAV_IO failed: %s\n", SDL_GetError());
        return;
    }
    if (wave_buffer == NULL || wave_length == 0) {
        rs2_error("SDL3: bad wave data\n");
        free(wave_buffer);
        return;
    }
    if (g_wavevol != 128) {
        for (uint32_t i = 0; i < wave_length; i++) {
            wave_buffer[i] = (wave_buffer[i] - 128) * g_wavevol / 128 + 128;
        }
    }
    const int wave_bytes_already_in_stream = SDL_GetAudioStreamAvailable(wave_stream);
    if (wave_bytes_already_in_stream == 0) {
        if (!SDL_PutAudioStreamData(wave_stream, wave_buffer, wave_length)) {
            rs2_error("SDL3: PutAudioStreamData(Wave) failed: %s\n", SDL_GetError());
            free(wave_buffer);
            return;
        }
        if (!SDL_ResumeAudioStreamDevice(wave_stream)) {
            rs2_error("SDL3: ResumeAudioStreamDevice(Wave) failed: %s\n", SDL_GetError());
            free(wave_buffer);
            return;
        }
    }
    free(wave_buffer);
}

void platform_set_wave_volume(int wavevol) {
    g_wavevol = wavevol;
}

static void midi_callback(void *data, SDL_AudioStream *stream, int additional_amount_needed, int total_amount_requested) {
    (void)data;
    (void)total_amount_requested;
    if (additional_amount_needed == 0) {
        return;
    }
    const float samples_per_second = 44100.0;
    const float samples_per_millisecond = 1000.0 / samples_per_second;
    // Number of samples per block
    const int num_samples_per_block = TSF_RENDER_EFFECTSAMPLEBLOCK;
    // Number of bytes per block
    const int num_bytes_per_block = num_samples_per_block * 2 * sizeof(float); // 2 channels F32
    // Number of bytes that still need to be provided to the Audio Stream; decreases by num_bytes_per_block each iteration.
    int num_bytes_remaining = additional_amount_needed;

    // Continually read Midi messages until we have no more bytes needed to be sent to the Audio Stream:
    while (num_bytes_remaining > 0) {
        // Note: over-feeding Audio Stream is OK, keep the blocks of uniform size.

        // Loop through all MIDI messages which need to be played up until the current playback time
        for (g_Msec += num_samples_per_block * samples_per_millisecond; g_MidiMessage && g_Msec >= g_MidiMessage->time; g_MidiMessage = g_MidiMessage->next) {
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

        // Provide one block of samples to the Audio Stream
        tsf_render_float(g_TinySoundFont, midi_buffer, num_samples_per_block, 0);
        if (!SDL_PutAudioStreamData(stream, midi_buffer, num_bytes_per_block)) {
            rs2_error("SDL3: PutAudioStreamData failed: %s\n", SDL_GetError());
        }
        num_bytes_remaining -= num_bytes_per_block;
    }
}

bool platform_init(void) {
    return true;
}

static bool onmousemove(int event_type, const EmscriptenMouseEvent *e, void *user_data);
static bool onmousedown(int event_type, const EmscriptenMouseEvent *e, void *user_data);
static bool onmouseup(int event_type, const EmscriptenMouseEvent *e, void *user_data);
static bool onmouseenter(int event_type, const EmscriptenMouseEvent *e, void *user_data);
static bool onmouseleave(int event_type, const EmscriptenMouseEvent *e, void *user_data);
static bool onfocus(int event_type, const EmscriptenFocusEvent *e, void *user_data);
static bool onblur(int event_type, const EmscriptenFocusEvent *e, void *user_data);
static bool onkeydown(int event_type, const EmscriptenKeyboardEvent *e, void *user_data);
static bool onkeyup(int event_type, const EmscriptenKeyboardEvent *e, void *user_data);

void platform_new(GameShell *shell) {
    emscripten_set_canvas_element_size("#canvas", shell->screen_width, shell->screen_height);

    emscripten_set_mousemove_callback("#canvas", shell, false, onmousemove);
    emscripten_set_mousedown_callback("#canvas", shell, false, onmousedown);
    emscripten_set_mouseup_callback("#canvas", shell, false, onmouseup);

    emscripten_set_keydown_callback("#canvas", shell, false, onkeydown);
    emscripten_set_keyup_callback("#canvas", shell, false, onkeyup);

    emscripten_set_mouseenter_callback("#canvas", shell, false, onmouseenter);
    emscripten_set_mouseleave_callback("#canvas", shell, false, onmouseleave);

    emscripten_set_focus_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, shell, false, onfocus);
    emscripten_set_blur_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, shell, false, onblur);

    // emscripten_set_touchstart_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, Emscripten_HandleTouch);
    // emscripten_set_touchend_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, Emscripten_HandleTouch);
    // emscripten_set_touchmove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, Emscripten_HandleTouch);
    // emscripten_set_touchcancel_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, Emscripten_HandleTouch);

    // emscripten_set_wheel_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, Emscripten_HandleWheel);

    // emscripten_set_fullscreenchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, c, false, Emscripten_HandleFullscreenChange);

    // emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, c, false, Emscripten_HandleResize);

    // emscripten_set_beforeunload_callback(c, Emscripten_HandleBeforeUnload);

    if (_Client.lowmem) {
        return;
    }

    if (!SDL_Init(SDL_INIT_AUDIO)) {
        rs2_error("SDL_Init failed: %s\n", SDL_GetError());
        return;
    }

    const SDL_AudioSpec midi_spec = {.format = SDL_AUDIO_F32, .channels = 2, .freq = 44100};

    void *buffer = NULL;
    int size = 0;
    int error = 0;
    emscripten_wget_data("SCC1_Florestan.sf2", &buffer, &size, &error);
    if (error) {
        emscripten_wget_data("rom/SCC1_Florestan.sf2", &buffer, &size, &error);
        if (error) {
            rs2_error("Error downloading SoundFont: %d\n", error);
        }
    }

    g_TinySoundFont = tsf_load_memory(buffer, size);
    if (!g_TinySoundFont) {
        rs2_error("Could not load SoundFont\n");
    } else {
        // Set the SoundFont rendering output mode
        tsf_set_output(g_TinySoundFont, TSF_STEREO_INTERLEAVED, midi_spec.freq, 0.0f);

        midi_buffer = malloc(TSF_RENDER_EFFECTSAMPLEBLOCK * 2 * sizeof(float));
        midi_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &midi_spec, midi_callback, NULL);
        if (!midi_stream) {
            rs2_error("SDL3: OpenAudioDeviceStream(Midi) failed: %s\n", SDL_GetError());
        }
        if (!SDL_ResumeAudioStreamDevice(midi_stream)) {
            rs2_error("SDL3: ResumeAudioStreamDevice failed: %s\n", SDL_GetError());
        }
    }

    // Create WAVE audio stream:
    const SDL_AudioSpec wave_spec = {.format = SDL_AUDIO_U8, .channels = 1, .freq = 22050};
    wave_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &wave_spec, NULL, NULL);
    if (!wave_stream) {
        rs2_error("SDL3: OpenAudioDeviceStream(Wave) failed: %s\n", SDL_GetError());
    }
}

void platform_free(void) {
}
void platform_set_midi_volume(float midivol) {
    if (_Client.lowmem) {
        return;
    }
    if (midi_stream) {
        tsf_set_volume(g_TinySoundFont, midivol);
    }
}
void platform_set_jingle(int8_t *src, int len) {
    platform_stop_midi();
    tml_free(TinyMidiLoader);
    TinyMidiLoader = tml_load_memory(src, len);
    g_MidiMessage = TinyMidiLoader;
    free(src);
}
void platform_set_midi(const char *name, int crc, int len) {
    char filename[PATH_MAX];
    void *data = NULL;
    int data_len = 0;
    snprintf(filename, sizeof(filename), "%s_%d.mid", name, crc);
    data = client_openurl(filename, &data_len);
    if (!data) {
        rs2_error("Error loading midi file %s (NOTE: authentic if empty when relogging?)\n", filename);
        return;
    }

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

    platform_stop_midi();
    tml_free(TinyMidiLoader);
    TinyMidiLoader = tml_load_memory(uncompressed, uncompressed_length);
    g_MidiMessage = TinyMidiLoader;

    packet_free(packet);
    free(uncompressed);
}
void platform_stop_midi(void) {
    if (_Client.lowmem) {
        return;
    }
    if (midi_stream) {
        g_MidiMessage = NULL;
        g_Msec = 0;
        tsf_reset(g_TinySoundFont);
        // Initialize preset on special 10th MIDI channel to use percussion sound bank (128) if available
        tsf_channel_set_bank_preset(g_TinySoundFont, 9, 128, 0);
    }
}
static bool onmousemove(int event_type, const EmscriptenMouseEvent *e, void *user_data) {
    (void)event_type;
    GameShell *shell = user_data;
    int x = e->clientX - frame_insets_left_js();
    int y = e->clientY - frame_insets_top_js();

    shell->idle_cycles = 0;
    shell->mouse_x = x;
    shell->mouse_y = y;

    if (_InputTracking.enabled) {
        inputtracking_mouse_moved(&_InputTracking, x, y);
    }
    return 0;
}

static bool onmousedown(int event_type, const EmscriptenMouseEvent *e, void *user_data) {
    (void)event_type;
    GameShell *shell = user_data;
    int x = e->clientX - frame_insets_left_js();
    int y = e->clientY - frame_insets_top_js();

    shell->idle_cycles = 0;
    shell->mouse_click_x = x;
    shell->mouse_click_y = y;

    if (e->button == 2) {
        shell->mouse_click_button = 2;
        shell->mouse_button = 2;
    } else {
        shell->mouse_click_button = 1;
        shell->mouse_button = 1;
    }

    if (_InputTracking.enabled) {
        inputtracking_mouse_pressed(&_InputTracking, x, y, e->button == 2 ? 1 : 0);
    }
    return 0;
}

static bool onmouseup(int event_type, const EmscriptenMouseEvent *e, void *user_data) {
    (void)event_type;
    GameShell *shell = user_data;
    shell->idle_cycles = 0;
    shell->mouse_button = 0;

    if (_InputTracking.enabled) {
        inputtracking_mouse_released(&_InputTracking, (e->button == 2) != 0 ? 1 : 0);
    }
    return 0;
}

static bool onmouseenter(int event_type, const EmscriptenMouseEvent *e, void *user_data) {
    (void)event_type, (void)e, (void)user_data;
    if (_InputTracking.enabled) {
        inputtracking_mouse_entered(&_InputTracking);
    }
    return 0;
}

static bool onmouseleave(int event_type, const EmscriptenMouseEvent *e, void *user_data) {
    (void)event_type, (void)e, (void)user_data;
    if (_InputTracking.enabled) {
        inputtracking_mouse_exited(&_InputTracking);
    }
    return 0;
}

static bool onfocus(int event_type, const EmscriptenFocusEvent *e, void *user_data) {
    (void)event_type, (void)e;
    GameShell *shell = user_data;

    shell->has_focus = true; // mapview applet
    shell->refresh = true;
#ifdef client
    // c->redraw_background = true; NOTE: is this really needed
#endif
#ifdef mapview
// TODO add mapview refresh
#endif
    if (_InputTracking.enabled) {
        inputtracking_focus_gained(&_InputTracking);
    }
    return 0;
}

static bool onblur(int event_type, const EmscriptenFocusEvent *e, void *user_data) {
    (void)event_type, (void)e;
    GameShell *shell = user_data;

    shell->has_focus = false; // mapview applet
    if (_InputTracking.enabled) {
        inputtracking_focus_lost(&_InputTracking);
    }
    return 0;
}

static void platform_get_keycodes(const EmscriptenKeyboardEvent *e, int *code, unsigned char *ch) {
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

    // java ctrl key lowers char value
    if (e->ctrlKey) {
        if ((*ch >= 'A' && *ch <= ']') || *ch == '_') {
            *ch -= 'A' - 1;
        } else if (*ch >= 'a' && *ch <= 'z') {
            *ch -= 'a' - 1;
        }
    }
}

static bool onkeydown(int event_type, const EmscriptenKeyboardEvent *e, void *user_data) {
    (void)event_type;
    GameShell *shell = user_data;

    int code = -1;
    unsigned char ch = -1;
    platform_get_keycodes(e, &code, &ch);
    key_pressed(shell, code, ch);

    if (strcmp(e->key, "F5") == 0 || strcmp(e->key, "F11") == 0 || strcmp(e->key, "F12") == 0) {
        return 0;
    }
    // returning 1 = preventDefault
    return 1;
}

static bool onkeyup(int event_type, const EmscriptenKeyboardEvent *e, void *user_data) {
    (void)event_type;
    GameShell *shell = user_data;

    int code = -1;
    unsigned char ch = -1;
    platform_get_keycodes(e, &code, &ch);
    key_released(shell, code, ch);

    if (strcmp(e->key, "F5") == 0 || strcmp(e->key, "F11") == 0 || strcmp(e->key, "F12") == 0) {
        return 0;
    }
    // returning 1 = preventDefault
    return 1;
}

void platform_poll_events(Client *c) {
    (void)c;
}
void platform_blit_surface(Surface *surface, int x, int y) {
    set_pixels_js(x, y, surface->w, surface->h, surface->pixels);
}
int platform_string_width(const char* str) {
    return string_width_js(str);
}
void platform_set_color(int color) {
    set_color_js(color);
}
void platform_set_font(const char* name, bool bold, int size) {
    set_font_js(name, bold, size);
}
void platform_draw_string(const char *str, int x, int y) {
    draw_string_js(str, x, y);
}
void platform_update_surface(void) {
    rs2_sleep(0); // return a slice of time to the main loop so it can update the progress bar
}
void platform_draw_rect(int x, int y, int w, int h) {
    draw_rect_js(x, y, w, h);
}
void platform_fill_rect(int x, int y, int w, int h) {
    fill_rect_js(x, y, w, h);
}
uint64_t rs2_now(void) {
    return emscripten_get_now();
}
void rs2_sleep(int ms) {
    emscripten_sleep(ms);
}
#endif
