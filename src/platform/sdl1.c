#if SDL == 1
#include "SDL.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "../client.h"
#include "../custom.h"
#include "../defines.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../pixmap.h"
#include "../gl11.h"

extern ClientData _Client;
extern InputTracking _InputTracking;
extern Custom _Custom;

static SDL_Surface *window_surface;

static void platform_get_keycodes(SDL_keysym *keysym, int *code, char *ch);

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

static bool g_WaveActive = false;
static uint8_t *g_WaveSamples = NULL;
static int g_WaveSampleCount = 0;
static int g_WaveSamplePos = 0;
static int g_WaveVolume = 128;

#define MIDI_FREQ 22050

static int16_t clamp_s16(int sample) {
    if (sample < -32768) {
        return -32768;
    }
    if (sample > 32767) {
        return 32767;
    }
    return sample;
}

static void midi_callback(void *data, Uint8 *stream, int len) {
    (void)data;
    memset(stream, 0, len); // SDL1 requires clearing the buffer manually

    // Number of samples to process (SDL1 only supports 16-bit audio)
    int SampleBlock, SampleCount = len / (2 * sizeof(int16_t)); // 2 output channels

    int16_t *mix = (int16_t*)stream;
    int total_samples = SampleCount;

    for (SampleBlock = TSF_RENDER_EFFECTSAMPLEBLOCK; SampleCount;
         SampleCount -= SampleBlock, stream += (SampleBlock * 2 * sizeof(int16_t))) {
        if (SampleBlock > SampleCount)
            SampleBlock = SampleCount;

        // Process MIDI messages
        for (g_Msec += SampleBlock * (1000.0 / MIDI_FREQ);
             g_MidiMessage && g_Msec >= g_MidiMessage->time;
             g_MidiMessage = g_MidiMessage->next) {
            switch (g_MidiMessage->type) {
            case TML_PROGRAM_CHANGE:
                tsf_channel_set_presetnumber(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->program, (g_MidiMessage->channel == 9));
                break;
            case TML_NOTE_ON:
                tsf_channel_note_on(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->key, g_MidiMessage->velocity / 127.0f);
                break;
            case TML_NOTE_OFF:
                tsf_channel_note_off(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->key);
                break;
            case TML_PITCH_BEND:
                tsf_channel_set_pitchwheel(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->pitch_bend);
                break;
            case TML_CONTROL_CHANGE:
                tsf_channel_midi_control(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->control, g_MidiMessage->control_value);
                break;
            }
        }

        // Render in 16-bit PCM format (SDL1 does not support float)
        tsf_render_short(g_TinySoundFont, (int16_t *)stream, SampleBlock, 0);
    }

    if (g_WaveSamples && g_WaveSamplePos < g_WaveSampleCount) {
        int wavevol = g_WaveVolume;
        if (wavevol < 0) {
            wavevol = 0;
        } else if (wavevol > 128) {
            wavevol = 128;
        }

        int remaining = g_WaveSampleCount - g_WaveSamplePos;
        int count = remaining > total_samples ? total_samples : remaining;

        for (int i = 0; i < count; i++) {
            int samples16 = (g_WaveSamples[g_WaveSamplePos + i] - 128) << 8;
            int wave = samples16 * wavevol / 128;

            int left = mix[i * 2] + wave;
            int right = mix[i * 2 + 1] + wave;
            mix[i * 2] = clamp_s16(left);
            mix[i * 2 + 1] = clamp_s16(right);
        }

        g_WaveSamplePos += count;
        if (g_WaveSamplePos >= g_WaveSampleCount) {
            SDL_FreeWAV(g_WaveSamples);
            g_WaveSamples = NULL;
            g_WaveSampleCount = 0;
            g_WaveSamplePos = 0;
            g_WaveActive = false;
        }
    }
}

bool platform_init(void) {
    int init = SDL_INIT_VIDEO;
    if (!_Client.lowmem) {
        init |= SDL_INIT_AUDIO;
    }
    if (SDL_Init(init) < 0) {
        rs2_error("SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

void platform_new(GameShell *shell) {
    SDL_EnableUNICODE(1);
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
    SDL_WM_SetCaption("Jagex", NULL);

#ifdef GL11
    // explicitly setting these fixes mesa d3d12/llvmpipe on windows for sdl1
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    int flags = SDL_OPENGL;
#else
    int flags = SDL_SWSURFACE;
#endif
    // flags |= SDL_RESIZABLE;
    window_surface = SDL_SetVideoMode(shell->screen_width, shell->screen_height, 32, flags);

    if (_Client.lowmem) {
        return;
    }

    // SDL1 only has 1 audio device, so wavs are mixed in the callback
    // SDL_AudioSpec wavSpec;
    // wavSpec.freq = 22050;
    // wavSpec.format = AUDIO_U8;
    // wavSpec.channels = 1;
    // wavSpec.samples = 128;
    // wavSpec.callback = NULL;

    SDL_AudioSpec midiSpec;
    midiSpec.freq = MIDI_FREQ;
    midiSpec.format = AUDIO_S16SYS;
    midiSpec.channels = 2;
    midiSpec.samples = 4096;
    midiSpec.callback = midi_callback;

    g_TinySoundFont = tsf_load_filename("rom/SCC1_Florestan.sf2");
    if (!g_TinySoundFont) {
        rs2_error("Could not load SoundFont\n");
    } else {
        // Set the SoundFont rendering output mode
        tsf_set_output(g_TinySoundFont, TSF_STEREO_INTERLEAVED, midiSpec.freq, 0.0f);

        if (SDL_OpenAudio(&midiSpec, NULL) < 0) {
            rs2_error("Could not open the audio hardware or the desired audio output format: %s\n", SDL_GetError());
        }
        SDL_PauseAudio(0);
    }
}

void platform_free(void) {
    SDL_Quit();
    tsf_close(g_TinySoundFont);
    tml_free(TinyMidiLoader);
}

void platform_play_wave(int8_t *src, int length) {
    if (!src || length > 2000000) {
        return;
    }

    SDL_AudioSpec wavSpec;
    uint8_t *wavBuffer;
    uint32_t wavLength;

    SDL_RWops *rw = SDL_RWFromMem(src, length);

    SDL_LoadWAV_RW(rw, 1, &wavSpec, &wavBuffer, &wavLength);

    // TODO precompute volume table just for 128 96 64 32?
    if (g_WaveVolume != 128) {
        for (uint32_t i = 0; i < wavLength; i++) {
            wavBuffer[i] = (wavBuffer[i] - 128) * g_WaveVolume / 128 + 128;
        }
    }

    if (!g_WaveActive) {
        g_WaveSamples = wavBuffer;
        g_WaveSampleCount = wavLength;
        g_WaveActive = true;
    }
}

void platform_set_wave_volume(int wavevol) {
    g_WaveVolume = wavevol;
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
    platform_stop_midi();
    tml_free(TinyMidiLoader);
    TinyMidiLoader = tml_load_memory(src, len);
    g_MidiMessage = TinyMidiLoader;
    free(src);
}

// TODO add fade (always, not jingles)
void platform_set_midi(const char *name, int crc, int len) {
    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "rom/cache/client/songs/%s.mid", name);
    FILE *file = fopen(filename, "rb");
    if (!file) {
        rs2_error("Error loading midi file %s: %s (NOTE: authentic if empty when relogging?)\n", filename, strerror(errno));
        return;
    }

    int8_t *data = malloc(len);
    const size_t data_len = fread(data, 1, len, file);
    fclose(file);
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
void platform_blit_surface(Surface *surface, int x, int y) {
#ifdef GL11
    (void)surface, (void)x, (void)y;
#else
    SDL_Rect dest = {x, y, surface->w, surface->h};
    SDL_BlitSurface(surface, NULL, window_surface, &dest);
#endif
}

void platform_update_surface(void) {
#ifdef GL11
    SDL_GL_SwapBuffers();
#else
    SDL_Flip(window_surface);
#endif
}

static void platform_get_keycodes(SDL_keysym *keysym, int *code, char *ch) {
    *code = -1;
    *ch = -1;

    switch (keysym->sym) {
    case SDLK_TAB:
        *code = K_TAB;
        *ch = '\t';
        break;
    case SDLK_BACKSPACE:
        *code = K_BACKSPACE;
        *ch = '\b';
        break;
    case SDLK_LEFT:
        *code = K_LEFT;
        break;
    case SDLK_RIGHT:
        *code = K_RIGHT;
        break;
    case SDLK_UP:
        *code = K_UP;
        break;
    case SDLK_DOWN:
        *code = K_DOWN;
        break;
    case SDLK_RCTRL:
    case SDLK_LCTRL:
        *code = K_CONTROL;
        break;
    case SDLK_PAGEUP:
        *code = K_PAGE_UP;
        break;
    case SDLK_PAGEDOWN:
        *code = K_PAGE_DOWN;
        break;
    case SDLK_END:
        *code = K_END;
        break;
    case SDLK_HOME:
        *code = K_HOME;
        break;
    case SDLK_F1:
        *code = K_F1;
        break;
    case SDLK_F2:
        *code = K_F2;
        break;
    case SDLK_F3:
        *code = K_F3;
        break;
    case SDLK_F4:
        *code = K_F4;
        break;
    case SDLK_F5:
        *code = K_F5;
        break;
    case SDLK_F6:
        *code = K_F6;
        break;
    case SDLK_F7:
        *code = K_F7;
        break;
    case SDLK_F8:
        *code = K_F8;
        break;
    case SDLK_F9:
        *code = K_F9;
        break;
    case SDLK_F10:
        *code = K_F10;
        break;
    case SDLK_F11:
        *code = K_F11;
        break;
    case SDLK_F12:
        *code = K_F12;
        break;
    case SDLK_ESCAPE:
        *code = K_ESCAPE;
        break;
    case SDLK_RETURN:
        *code = K_ENTER;
        *ch = '\r';
        break;
    case SDLK_KP1:
    case SDLK_1:
        *code = K_1;
        *ch = K_1;
        break;
    case SDLK_KP2:
    case SDLK_2:
        *code = K_2;
        *ch = K_2;
        break;
    case SDLK_KP3:
    case SDLK_3:
        *code = K_3;
        *ch = K_3;
        break;
    case SDLK_KP4:
    case SDLK_4:
        *code = K_4;
        *ch = K_4;
        break;
    case SDLK_KP5:
    case SDLK_5:
        *code = K_5;
        *ch = K_5;
        break;
    case SDLK_BACKQUOTE:
        *code = SDLK_BACKQUOTE;
        if (keysym->mod & KMOD_SHIFT) {
            *ch = '~';
        } else {
            *ch = '`';
        }
        break;
    case SDLK_QUOTE:
        *code = 222;
        *ch = '\'';
        break;
    case SDLK_QUOTEDBL:
        *code = 222;
        *ch = '"';
        break;
    default:
        /* NOTE: unicode is not set for key released */
        if (keysym->unicode > 0 && keysym->unicode < 128) {
            if (isprint((unsigned char)keysym->unicode)) {
                *code = keysym->unicode;
                *ch = keysym->unicode;
                return;
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
        case SDL_ACTIVEEVENT:
            if (e.active.state & SDL_APPMOUSEFOCUS) {
                if (e.active.gain) {
                    if (_InputTracking.enabled) {
                        inputtracking_mouse_entered(&_InputTracking);
                    }
                } else {
                    if (_InputTracking.enabled) {
                        inputtracking_mouse_exited(&_InputTracking);
                    }
                }
            }
            if (e.active.state & SDL_APPINPUTFOCUS) {
                if (e.active.gain) {
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
                } else {
                    c->shell->has_focus = false; // mapview applet
                    if (_InputTracking.enabled) {
                        inputtracking_focus_lost(&_InputTracking);
                    }
                }
            }
            // if (e.active.state & SDL_APPACTIVE) { // TODO: doesn't always work
            break;
        case SDL_VIDEORESIZE:
        break;
        }
    }
}

uint64_t rs2_now(void) {
    return SDL_GetTicks();
}

void rs2_sleep(int ms) {
    SDL_Delay(ms);
}
#endif
