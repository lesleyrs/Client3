#if SDL == 2
#include "SDL.h"

#include <stdio.h>
#include <string.h>

#include "../client.h"
#include "../defines.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../pixmap.h"

extern ClientData _Client;
extern InputTracking _InputTracking;

#include "../thirdparty/bzip.h"
#define TSF_IMPLEMENTATION
#include "../thirdparty/tsf.h"

#define TML_IMPLEMENTATION
#include "../thirdparty/tml.h"

static tml_message *TinyMidiLoader = NULL;

// Holds the global instance pointer
static tsf *g_TinySoundFont;
// Holds global MIDI playback state
static double g_Msec;              // current playback time
static tml_message *g_MidiMessage; // next message to be played

// TODO separate sdl1? or separate midi
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

void platform_new(GameShell *shell, int width, int height) {
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    // SDL_SetHint(SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4, "1"); // OSRS desktop client always had this, not sure about before?

    int init = SDL_INIT_VIDEO;
    if (!_Client.lowmem) {
        init |= SDL_INIT_AUDIO;
    }
    if (SDL_Init(init) < 0) {
        rs2_error("SDL_Init failed: %s\n", SDL_GetError());
        return;
    }

    if (!_Client.lowmem) {
        SDL_AudioSpec midiSpec;
        midiSpec.freq = 44100;
// TODO separate midi and rm from sdl2
#if SDL == 1
        midiSpec.format = AUDIO_S16SYS;
#else
        midiSpec.format = AUDIO_F32;
#endif
        midiSpec.channels = 2;
        midiSpec.samples = 4096;
        midiSpec.callback = midi_callback;

        g_TinySoundFont = tsf_load_filename("SCC1_Florestan.sf2");
        if (!g_TinySoundFont) {
            rs2_error("Could not load SoundFont\n");
            exit(1);
        }

        // Set the SoundFont rendering output mode
        tsf_set_output(g_TinySoundFont, TSF_STEREO_INTERLEAVED, midiSpec.freq, 0.0f);

        // if (SDL_OpenAudio(&midiSpec, NULL) < 0) {
        SDL_AudioDeviceID device = SDL_OpenAudioDevice(NULL, 0, &midiSpec, NULL, 0);
        if (!device) {
            rs2_error("Could not open the audio hardware or the desired audio output format\n");
        }

        SDL_PauseAudioDevice(device, 0);
        // }

#ifndef __EMSCRIPTEN__
        SDL_AudioSpec wavSpec;
        wavSpec.freq = 22050;
        wavSpec.format = AUDIO_U8;
        wavSpec.channels = 1;
        wavSpec.samples = 4096;
        wavSpec.callback = NULL;

        if (SDL_OpenAudio(&wavSpec, NULL) < 0) {
            rs2_error("SDL_OpenAudio(): %s\n", SDL_GetError());
        }
#endif
    }

    shell->window = SDL_CreateWindow("Jagex", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
    if (!shell->window) {
        rs2_error("Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return;
    }

    // TODO all platforms SDL_FreeSurface(shell->surface); // https://wiki.libsdl.org/SDL3/SDL_GetWindowSurface#Remarks
    shell->surface = SDL_GetWindowSurface(shell->window);
    if (!shell->surface) {
        rs2_error("Window surface creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(shell->window);
        SDL_Quit();
        return;
    }

    // SDL_Surface renderer = SDL_CreateSoftwareRenderer()
    // SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    // if (!renderer) {
    //     rs2_error("Renderer creation failed: %s\n", SDL_GetError());
    //     SDL_DestroyWindow(window);
    //     SDL_Quit();
    //     return;
    // }
}

void platform_free(GameShell *shell) {
    // SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(shell->window);
    SDL_Quit();
    tsf_close(g_TinySoundFont);
    tml_free(TinyMidiLoader);
}

void platform_set_midi_volume(float midivol) {
    if (_Client.lowmem) {
        return;
    }
    tsf_set_volume(g_TinySoundFont, midivol);
}

void platform_set_jingle(int8_t *src, int len) {
    // tml_free(TinyMidiLoader);
    TinyMidiLoader = tml_load_memory(src, len);
    platform_stop_midi();
    g_MidiMessage = TinyMidiLoader;
    free(src);
}

// TODO add fade (always, not jingles)
void platform_set_midi(const char *name, int crc, int len) {
    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "cache/client/songs/%s.mid", name);
    FILE *file = fopen(filename, "rb");
    if (!file) {
        rs2_error("Error loading midi file %s: %s (NOTE: authentic if empty?)\n", filename, strerror(errno));
        return;
    }

    int8_t *data = malloc(len);
    const size_t data_len = fread(data, 1, len, file);
    if (data && crc != 12345678) {
        int data_crc = crc32(data, len);
        if (data_crc != crc) {
            free(data);
            data = NULL;
        }
    }

    Packet *packet = packet_new(data, 4);
    const int uncompressed_length = g4(packet);
    int8_t *uncompressed = malloc(uncompressed_length);
    bzip_decompress(uncompressed, data, (int)data_len - 4, 4);
    // tml_free(TinyMidiLoader);
    TinyMidiLoader = tml_load_memory(uncompressed, uncompressed_length);
    platform_stop_midi();
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

#ifdef __EMSCRIPTEN__
#include "emscripten.h"

EM_JS(void, set_wave_volume_js, (int wavevol), {
    setWaveVolume(wavevol);
})

EM_JS(void, play_wave_js, (int8_t * src, int length), {
    playWave(HEAP8.subarray(src, src + length));
})

void platform_set_wave_volume(int wavevol) {
    set_wave_volume_js(wavevol);
}

void platform_play_wave(int8_t *src, int length) {
    play_wave_js(src, length);
}

#else
static int g_wavevol = 128;

void platform_set_wave_volume(int wavevol) {
    g_wavevol = wavevol;
}

void platform_play_wave(int8_t *src, int length) {
    if (!src || length > 2000000) {
        rs2_log("TODO does this ever happen: wav buffer empty or too large: %i\n", length);
        return;
    }

    SDL_AudioSpec wavSpec;
    uint8_t *wavBuffer;
    uint32_t wavLength;

    SDL_RWops *rw = SDL_RWFromMem(src, length);

    SDL_LoadWAV_RW(rw, 1, &wavSpec, &wavBuffer, &wavLength);

    // TODO precompute volume table just for 128 96 64 32?
    if (g_wavevol != 128) {
        for (uint32_t i = 0; i < wavLength; i++) {
            wavBuffer[i] = (wavBuffer[i] - 128) * g_wavevol / 128 + 128;
        }
    }

    // TODO rm
    // rs2_log("wav %i %i %i\n", wavSpec.freq, wavSpec.samples, wavSpec.format);
    // SDL_AudioDeviceID deviceId = SDL_OpenAudioDevice(NULL, 0, &wavSpec, NULL, 0);
    // int success = SDL_QueueAudio(deviceId, wavBuffer, wavLength);
    // SDL_PauseAudioDevice(deviceId, 0);
    // TODO: custom option for having multiple audio devices play sounds at same time (inauthentic)
    // TODO: web already plays multiple sounds at once as it uses different code (inauthentic)
    if (SDL_GetQueuedAudioSize(1) == 0) {
        SDL_QueueAudio(1, wavBuffer, wavLength);
        SDL_PauseAudio(0);
    }
    SDL_FreeWAV(wavBuffer);
}
#endif /* not EMSCRIPTEN */

Surface *platform_create_surface(int *pixels, int width, int height, int alpha) {
    return SDL_CreateRGBSurfaceFrom(pixels, width, height, 32, width * sizeof(int), 0xff0000, 0x00ff00, 0x0000ff, alpha);
}

void platform_free_surface(Surface *surface) {
    SDL_FreeSurface(surface);
}

int *get_pixels(Surface *surface) {
    return surface->pixels;
}

void set_pixels(PixMap *pixmap, int x, int y) {
    SDL_Rect dest = {x, y, pixmap->width, pixmap->height};
    SDL_BlitScaled(pixmap->image, NULL, pixmap->shell->surface, &dest);
    // SDL_BlitScaled(pixmap->image, NULL, pixmap->shell->surface, NULL);
    // SDL_BlitSurface(pixmap->image, NULL, pixmap->shell->surface, NULL);
    SDL_UpdateWindowSurface(pixmap->shell->window);
}

void platform_blit_surface(GameShell *shell, int x, int y, int w, int h, Surface *surface) {
    SDL_Rect rect = {x, y, w, h};
    SDL_BlitScaled(surface, NULL, shell->surface, &rect);
    // SDL_BlitSurface(surface, NULL, shell->surface, &rect);
}

void platform_update_surface(GameShell *shell) {
    SDL_UpdateWindowSurface(shell->window);
}

void platform_fill_rect(GameShell *shell, int x, int y, int w, int h, int color) {
    if (color != BLACK) { // TODO other grayscale?
        color = SDL_MapRGB(shell->surface->format, color >> 16 & 0xff, color >> 8 & 0xff, color & 0xff);
    }

    SDL_Rect rect = {x, y, w, h};
    SDL_FillRect(shell->surface, &rect, color);
}

#define K_LEFT 37
#define K_RIGHT 39
#define K_UP 38
#define K_DOWN 40
#define K_PAGE_UP 33
#define K_PAGE_DOWN 34
#define K_HOME 36
#define K_F1 112
#define K_ENTER 13

#define K_BACKSPACE 8
#define K_ESCAPE 27
#define K_TAB 9

#define K_FWD_SLASH 47
#define K_ASTERISK 42
#define K_MINUS 45
#define K_PLUS 43
#define K_PERIOD 46

#define K_0 48
#define K_1 49
#define K_2 50
#define K_3 51
#define K_4 52
#define K_5 53
#define K_6 54
#define K_7 55
#define K_8 56
#define K_9 57

void platform_get_keycodes(SDL_Keysym *keysym, int *code, char *ch) {
    *ch = -1;

    switch (keysym->scancode) {
    case SDL_SCANCODE_LEFT:
        *code = K_LEFT;
        break;
    case SDL_SCANCODE_RIGHT:
        *code = K_RIGHT;
        break;
    case SDL_SCANCODE_UP:
        *code = K_UP;
        break;
    case SDL_SCANCODE_DOWN:
        *code = K_DOWN;
        break;
    case SDL_SCANCODE_PAGEUP:
        *code = K_PAGE_UP;
        break;
    case SDL_SCANCODE_PAGEDOWN:
        *code = K_PAGE_DOWN;
        break;
    case SDL_SCANCODE_HOME:
        *code = K_HOME;
        break;
    case SDL_SCANCODE_F1:
        *code = K_F1;
        break;
    case SDL_SCANCODE_ESCAPE:
        *code = K_ESCAPE;
        break;
    /*case SDL_SCANCODE_RETURN:
        *code = K_ENTER;
        break;*/
    // TODO: Swallow "bad inputs" by default? ie. numlock, capslock
    case SDL_SCANCODE_NUMLOCKCLEAR:
        *code = -1;
        *ch = 1;
        break;
    case SDL_SCANCODE_CAPSLOCK:
        *code = -1;
        *ch = 1;
        break;
    case SDL_SCANCODE_KP_DIVIDE:
        *code = K_FWD_SLASH;
        *ch = K_FWD_SLASH;
        break;
    case SDL_SCANCODE_KP_MULTIPLY:
        *code = K_ASTERISK;
        *ch = K_ASTERISK;
        break;
    case SDL_SCANCODE_KP_MINUS:
        *code = K_MINUS;
        *ch = K_MINUS;
        break;
    case SDL_SCANCODE_KP_PLUS:
        *code = K_PLUS;
        *ch = K_PLUS;
        break;
    case SDL_SCANCODE_KP_PERIOD:
        *code = K_PERIOD;
        *ch = K_PERIOD;
        break;
    case SDL_SCANCODE_KP_ENTER:
        *code = K_ENTER;
        *ch = K_ENTER;
        break;
    case SDL_SCANCODE_KP_0:
        *code = K_0;
        *ch = K_0;
        break;
    case SDL_SCANCODE_KP_1:
        *code = K_1;
        *ch = K_1;
        break;
    case SDL_SCANCODE_KP_2:
        *code = K_2;
        *ch = K_2;
        break;
    case SDL_SCANCODE_KP_3:
        *code = K_3;
        *ch = K_3;
        break;
    case SDL_SCANCODE_KP_4:
        *code = K_4;
        *ch = K_4;
        break;
    case SDL_SCANCODE_KP_5:
        *code = K_5;
        *ch = K_5;
        break;
    case SDL_SCANCODE_KP_6:
        *code = K_6;
        *ch = K_6;
        break;
    case SDL_SCANCODE_KP_7:
        *code = K_7;
        *ch = K_7;
        break;
    case SDL_SCANCODE_KP_8:
        *code = K_8;
        *ch = K_8;
        break;
    case SDL_SCANCODE_KP_9:
        *code = K_9;
        *ch = K_9;
        break;
    default:
        *ch = keysym->sym;

        switch (keysym->scancode) {
        case SDL_SCANCODE_TAB:
            *code = K_TAB;
            break;
        case SDL_SCANCODE_1:
            *code = K_1;
            break;
        case SDL_SCANCODE_2:
            *code = K_2;
            break;
        case SDL_SCANCODE_3:
            *code = K_3;
            break;
        case SDL_SCANCODE_4:
            *code = K_4;
            break;
        case SDL_SCANCODE_5:
            *code = K_5;
            break;
        default:
            // TODO
            // rs2_log("code %i %i %i\n", keysym->scancode, 'w', 's');
            *code = *ch;
            break;
        }

        if (keysym->mod & KMOD_SHIFT) {
            if (*ch >= 'a' && *ch <= 'z') {
                *ch -= 32;
            } else {
                switch (*ch) {
                case ';':
                    *ch = ':';
                    break;
                case '`':
                    *ch = '~';
                    break;
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
                case '-':
                    *ch = '_';
                    break;
                case '=':
                    *ch = '+';
                    break;
                case '[':
                    *ch = '{';
                    break;
                case ']':
                    *ch = '}';
                    break;
                case '\\':
                    *ch = '|';
                    break;
                case ',':
                    *ch = '<';
                    break;
                case '.':
                    *ch = '>';
                    break;
                case '/':
                    *ch = '?';
                    break;
                }
            }
        }

        break;
    }
}

void platform_poll_events(Client *c) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            gameshell_destroy(c);
            break;
        case SDL_KEYDOWN: {
            char ch;
            int code;
            platform_get_keycodes(&e.key.keysym, &code, &ch);
            key_pressed(c->shell, code, ch);
            break;
        }
        case SDL_KEYUP: {
            char ch;
            int code;
            platform_get_keycodes(&e.key.keysym, &code, &ch);
            key_released(c->shell, code, ch);
            break;
        }
        case SDL_MOUSEMOTION: {
            int x = e.motion.x;
            int y = e.motion.y;

            c->shell->idle_cycles = 0;
            c->shell->mouse_x = x;
            c->shell->mouse_y = y;

            if (_InputTracking.enabled) {
                inputtracking_mouse_moved(&_InputTracking, x, y);
            }
        } break;
        case SDL_MOUSEBUTTONDOWN: {
            int x = e.button.x;
            int y = e.button.y;

            c->shell->idle_cycles = 0;
            c->shell->mouse_click_x = x;
            c->shell->mouse_click_y = y;

            if (e.button.button == SDL_BUTTON_RIGHT) {
                c->shell->mouse_click_button = e.button.button;

                c->shell->mouse_click_button = 2;
                c->shell->mouse_button = 2;
            } else {
                c->shell->mouse_click_button = 1;
                c->shell->mouse_button = 1;
            }

            if (_InputTracking.enabled) {
                inputtracking_mouse_pressed(&_InputTracking, x, y, e.button.button == SDL_BUTTON_RIGHT ? 1 : 0);
            }
        } break;
        case SDL_MOUSEBUTTONUP:
            c->shell->idle_cycles = 0;
            c->shell->mouse_button = 0;

            if (_InputTracking.enabled) {
                inputtracking_mouse_released(&_InputTracking, (e.button.button & SDL_BUTTON_RMASK) != 0 ? 1 : 0);
            }
            break;
        case SDL_WINDOWEVENT:
            switch (e.window.event) {
            case SDL_WINDOWEVENT_RESIZED:
                c->shell->surface = SDL_GetWindowSurface(c->shell->window);
                if (!c->shell->surface) {
                    rs2_error("Window surface creation failed: %s\n", SDL_GetError());
                    SDL_DestroyWindow(c->shell->window);
                    SDL_Quit();
                    return;
                }
                c->redraw_background = true;
                break;
            case SDL_WINDOWEVENT_ENTER:
                if (_InputTracking.enabled) {
                    inputtracking_mouse_entered(&_InputTracking);
                }
                break;
            case SDL_WINDOWEVENT_LEAVE:
                if (_InputTracking.enabled) {
                    inputtracking_mouse_exited(&_InputTracking);
                }
                break;
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                c->shell->has_focus = true; // mapview applet
                c->shell->refresh = true;
#ifdef client
                c->redraw_background = true;
#endif
#ifdef mapview
// TODO add mapview refresh
#endif
                if (_InputTracking.enabled) {
                    inputtracking_focus_gained(&_InputTracking);
                }
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
                c->shell->has_focus = false; // mapview applet
                if (_InputTracking.enabled) {
                    inputtracking_focus_lost(&_InputTracking);
                }
                break;
            }
            break;
        }
    }
}

int64_t get_ticks(void) {
    return SDL_GetTicks64();
}

void delay_ticks(int ticks) {
    SDL_Delay(ticks);
}

void rs2_log(const char *format, ...) {
    va_list args;
    va_start(args, format);

#if !defined(_3DS) && !defined(WII) && !defined(SDL12)
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, format,
                    args);
#else
    vprintf(format, args);
#endif

    va_end(args);
}

void rs2_error(const char *format, ...) {
    va_list args;
    va_start(args, format);

#if !defined(_3DS) && !defined(WII) && !defined(SDL12)
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR,
                    format, args);
#else
    vfprintf(stderr, format, args);
#endif

    va_end(args);
}
#endif
