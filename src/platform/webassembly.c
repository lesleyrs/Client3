#if defined(__wasm) && !defined(__EMSCRIPTEN__)
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <js/glue.h>
#include <js/dom_pk_codes.h>
#include <js/audio.h>

#include "../clientstream.h"
#include "../defines.h"
#include "../pixmap.h"
#include "../platform.h"
#include "../client.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../custom.h"

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

extern ClientData _Client;
extern Custom _Custom;
extern InputTracking _InputTracking;

uint32_t canvas[SCREEN_WIDTH * SCREEN_HEIGHT];

// TODO missing clang compiler-rt stub
int __unordtf2(int64_t a, int64_t b, int64_t c, int64_t d);
int __unordtf2(int64_t a, int64_t b, int64_t c, int64_t d) {
    (void)a, (void)b, (void)c, (void)d;
    return 0;
}

// NOTE: using more samples to avoid starving the midi stream running on main thread
#define MIDI_FREQ 44100
#define MIDI_SAMPLES 4096
uint8_t midi_stream[MIDI_SAMPLES];

static void midi_callback(void *userdata, uint8_t *stream, int len) {
    (void)userdata;
    // Number of samples to process
    int SampleBlock, SampleCount = (len / (2 * sizeof(float))); // 2 output channels
    for (SampleBlock = TSF_RENDER_EFFECTSAMPLEBLOCK; SampleCount; SampleCount -= SampleBlock, stream += (SampleBlock * (2 * sizeof(float)))) {
        // We progress the MIDI playback and then process TSF_RENDER_EFFECTSAMPLEBLOCK samples at once
        if (SampleBlock > SampleCount)
            SampleBlock = SampleCount;

        // Loop through all MIDI messages which need to be played up until the current playback time
        for (g_Msec += SampleBlock * (1000.0 / MIDI_FREQ); g_MidiMessage && g_Msec >= g_MidiMessage->time; g_MidiMessage = g_MidiMessage->next) {
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
        tsf_render_float(g_TinySoundFont, (float *)stream, SampleBlock, 0);
    }
}

static bool onmousemove(void *userdata, int x, int y) {
    GameShell *shell = userdata;
    shell->idle_cycles = 0;
    shell->mouse_x = x;
    shell->mouse_y = y;

    if (_InputTracking.enabled) {
        inputtracking_mouse_moved(&_InputTracking, x, y);
    }
    return 0;
}

static void onmouse(void *userdata, bool pressed, int button) {
    GameShell *shell = userdata;
    shell->idle_cycles = 0;

    if (pressed) {
        shell->mouse_click_x = shell->mouse_x;
        shell->mouse_click_y = shell->mouse_y;

        if (button == 2) {
            shell->mouse_click_button = 2;
            shell->mouse_button = 2;
        } else {
            shell->mouse_click_button = 1;
            shell->mouse_button = 1;
        }

        if (_InputTracking.enabled) {
            inputtracking_mouse_pressed(&_InputTracking, shell->mouse_x, shell->mouse_y, button == 2 ? 1 : 0);
        }
    } else {
        shell->mouse_button = 0;

        if (_InputTracking.enabled) {
            inputtracking_mouse_released(&_InputTracking, (button == 2) != 0 ? 1 : 0);
        }
    }
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

static bool onkey(void *userdata, bool pressed, int key, int code, int modifiers) {
    GameShell *shell = userdata;

    int rc = convert_pk(shell, key, code, modifiers, pressed);

    if (_InputTracking.enabled) {
        if (pressed) {
            inputtracking_key_pressed(&_InputTracking, key);
        } else {
            inputtracking_key_released(&_InputTracking, key);
        }
    }
    return rc;
}

static void onblur(void *userdata) {
    GameShell *shell = userdata;
    for (int i = 0; i < 128; i++) {
        shell->action_key[i] = 0;
    }
}

bool platform_init(void) { return true; }
void platform_new(GameShell *shell) {
    JS_createCanvas(shell->screen_width, shell->screen_height, "2d");
    // JS_setTitle("Jagex");
    JS_setTitle("RuneScape Game"); // from the html page
    JS_addMouseEventListener(shell, onmouse, onmousemove, NULL);
    JS_addKeyEventListener(shell, onkey);
    JS_addBlurEventListener(shell, onblur);

    if (_Client.lowmem) {
        return;
    }

    g_TinySoundFont = tsf_load_filename("SCC1_Florestan.sf2");
    if (!g_TinySoundFont) {
        g_TinySoundFont = tsf_load_filename("rom/SCC1_Florestan.sf2");
    }

    if (!g_TinySoundFont) {
        rs2_error("Could not load SoundFont\n");
    } else {
        // Set the SoundFont rendering output mode
        tsf_set_output(g_TinySoundFont, TSF_STEREO_INTERLEAVED, MIDI_FREQ, 0.0f);
        JS_resumeAudio(MIDI_FREQ);
        JS_setAudioCallback(midi_callback, NULL, midi_stream, MIDI_SAMPLES);
    }
}
void platform_free(void) {
    tsf_close(g_TinySoundFont);
    tml_free(TinyMidiLoader);
}
void platform_set_wave_volume(int wavevol) {
    JS_setAudioVolume((double)wavevol / INT8_MAX);
}
void platform_play_wave(int8_t *src, int length) {
    JS_startAudio((uint8_t*)src, length);
}
void platform_set_midi_volume(float midivol) {
    tsf_set_volume(g_TinySoundFont, midivol);
}
void platform_set_jingle(int8_t *src, int len) {
    platform_stop_midi();
    tml_free(TinyMidiLoader);
    TinyMidiLoader = tml_load_memory(src, len);
    g_MidiMessage = TinyMidiLoader;
    free(src);
}
// TODO add fade (always, not jingles)
void platform_set_midi(const char *name, int crc, int len) {
    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "%s_%d.mid", name, crc);

    // custom full url
    bool secured = false;
    char url[PATH_MAX];
    sprintf(url, "%s://%s:%d/%s", secured ? "https" : "http", _Client.socketip, _Custom.http_port, filename);

    FILE *file = fopen(url, "rb");
    if (!file) {
        rs2_error("Error loading midi file %s (NOTE: authentic if empty when relogging?)\n", filename);
        return;
    }

    int8_t *data = malloc(len);
    const size_t data_len = fread(data, 1, len, file);
    if (data && crc != 12345678) {
        int data_crc = rs_crc32(data, len);
        if (data_crc != crc) {
            rs2_log("%s midi CRC check failed\n", name);
            free(data);
            data = NULL;
        }
    }

    Packet *packet = packet_new(data, 4);
    const int uncompressed_length = g4(packet);
    int8_t *uncompressed = malloc(uncompressed_length);
    bzip_decompress(uncompressed, data, (int)data_len - 4, 4);

    platform_stop_midi();
    tml_free(TinyMidiLoader);
    TinyMidiLoader = tml_load_memory(uncompressed, uncompressed_length);
    g_MidiMessage = TinyMidiLoader;

    packet_free(packet);
    free(uncompressed);
    fclose(file);
}

void platform_stop_midi(void) {
    if (_Client.lowmem) {
        return;
    }

    g_MidiMessage = NULL;
    g_Msec = 0;
    tsf_reset(g_TinySoundFont);
    // Initialize preset on special 10th MIDI channel to use percussion sound bank (128) if available
    tsf_channel_set_bank_preset(g_TinySoundFont, 9, 128, 0);
}
void platform_poll_events(Client *c) {
    (void)c;
}
void platform_set_color(int color) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%06x", color);
    JS_fillStyle(buf);
    JS_strokeStyle(buf);
}
void platform_set_font(const char* name, bool bold, int size) {
    char buf[MAX_STR];
    sprintf(buf, "%s %dpx %s, sans-serif", bold ? "bold" : "normal", size, name);
    JS_setFont(buf);
}
int platform_string_width(const char *str) {
    return JS_measureTextWidth(str);
}
void platform_draw_string(const char *str, int x, int y) {
    JS_fillText(str, x, y);
}
void platform_blit_surface(Surface *surface, int x, int y) {
    platform_set_pixels(canvas, surface, x, y, true);
    JS_setPixelsAlpha(canvas);
}
void platform_update_surface(void) {
    rs2_sleep(0); // return a slice of time to the main loop so it can update the progress bar
}
void platform_draw_rect(int x, int y, int w, int h) {
    JS_strokeRect(x, y, w, h);
}
void platform_fill_rect(int x, int y, int w, int h) {
    JS_fillRect(x, y, w, h);
}
uint64_t rs2_now(void) {
    return JS_performanceNow();
}
void rs2_sleep(int ms) {
    JS_setTimeout(ms);
}
#endif
