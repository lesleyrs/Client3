#if SDL == 1
#include "SDL.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "../client.h"
#include "../custom.h"
#include "../defines.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../pixmap.h"

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

static void midi_callback(void *data, Uint8 *stream, int len) {
    (void)data;
    memset(stream, 0, len); // SDL1 requires clearing the buffer manually

    // Number of samples to process (SDL1 only supports 16-bit audio)
    int SampleBlock, SampleCount = len / (2 * sizeof(int16_t)); // 2 output channels

    for (SampleBlock = TSF_RENDER_EFFECTSAMPLEBLOCK; SampleCount;
         SampleCount -= SampleBlock, stream += (SampleBlock * 2 * sizeof(int16_t))) {

        if (SampleBlock > SampleCount)
            SampleBlock = SampleCount;

        // Process MIDI messages
        for (g_Msec += SampleBlock * (1000.0 / 44100.0);
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
}

bool platform_init(void) {
    return true;
}

void platform_new(int width, int height) {
    int init = SDL_INIT_VIDEO;
    if (!_Client.lowmem) {
        init |= SDL_INIT_AUDIO;
    }
    if (SDL_Init(init) < 0) {
        rs2_error("SDL_Init failed: %s\n", SDL_GetError());
        return;
    }

    // NOTE: input isn't compatible with sdl2 :(
    // SDL_EnableUNICODE(1);
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
    SDL_WM_SetCaption("Jagex", NULL);
    // window_surface = SDL_SetVideoMode(width, height, 32, SDL_HWSURFACE | SDL_RESIZABLE);
    window_surface = SDL_SetVideoMode(width, height, 32, SDL_SWSURFACE);

    if (_Client.lowmem) {
        return;
    }

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
    } else {
        // Set the SoundFont rendering output mode
        tsf_set_output(g_TinySoundFont, TSF_STEREO_INTERLEAVED, midiSpec.freq, 0.0f);

        if (SDL_OpenAudio(&midiSpec, NULL) < 0) {
            rs2_error("Could not open the audio hardware or the desired audio output format\n");
        }
        SDL_PauseAudio(0);
    }

    // TODO wavs (sdl1 only has 1 device)
    // SDL_AudioSpec wavSpec;
    // wavSpec.freq = 22050;
    // wavSpec.format = AUDIO_U8;
    // wavSpec.channels = 1;
    // wavSpec.samples = 4096;
    // wavSpec.callback = NULL;
}

void platform_free(void) {
    SDL_Quit();
    tsf_close(g_TinySoundFont);
    tml_free(TinyMidiLoader);
}

void platform_play_wave(int8_t *src, int length) {
}

void platform_set_wave_volume(int wavevol) {
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
        int data_crc = rs_crc32(data, len);
        if (data_crc != crc) {
            rs2_error("%s midi CRC check failed\n", name);
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
    if (SDL_GetAudioStatus() != SDL_AUDIO_STOPPED) {
        g_MidiMessage = NULL;
        g_Msec = 0;
        tsf_reset(g_TinySoundFont);
        // Initialize preset on special 10th MIDI channel to use percussion sound bank (128) if available
        tsf_channel_set_bank_preset(g_TinySoundFont, 9, 128, 0);
    }
}
void set_pixels(PixMap *pixmap, int x, int y) {
    platform_blit_surface(x, y, pixmap->width, pixmap->height, pixmap->image);
    platform_update_surface();
}

void platform_blit_surface(int x, int y, int w, int h, Surface *surface) {
    SDL_Rect dest = {x, y, w, h};
    SDL_BlitSurface(surface, NULL, window_surface, &dest);
}

void platform_update_surface(void) {
    SDL_Flip(window_surface);
}

void platform_draw_rect(int x, int y, int w, int h, int color) {
    uint32_t *pixels = (uint32_t *)window_surface->pixels;
    int width = window_surface->pitch / sizeof(uint32_t); // SCREEN_WIDTH

    for (int i = 0; i < w; i++) {
        pixels[y * width + x + i] = color;             // top
        pixels[((y + h - 1) * width) + x + i] = color; // bottom
    }

    for (int i = 0; i < h; i++) {
        pixels[(y + i) * width + x] = color;         // left
        pixels[(y + i) * width + x + w - 1] = color; // right
    }
}

void platform_fill_rect(int x, int y, int w, int h, int color) {
    SDL_Rect rect = {x, y, w, h};
    SDL_FillRect(window_surface, &rect, color);
}

static void platform_get_keycodes(SDL_keysym *keysym, int *code, char *ch) {
    *code = -1;
    *ch = -1;

    /* note: unicode is not set for key released */
    if (keysym->unicode > 0 && keysym->unicode < 128) {
        if (isprint((unsigned char)keysym->unicode)) {
            *code = keysym->unicode;
            *ch = keysym->unicode;
            return;
        }
    }

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
    default:
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
            // if (e.active.state & SDL_APPACTIVE) { // NOTE: doesn't always work
            break;
        }
    }
}

uint64_t get_ticks(void) {
    return SDL_GetTicks();
}

void delay_ticks(int ticks) {
    SDL_Delay(ticks);
}
#endif
