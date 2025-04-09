#if SDL == 3
#include "SDL3/SDL.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../client.h"
#include "../custom.h"
#include "../defines.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../pixmap.h"

#include "../thirdparty/bzip.h"
#define TSF_IMPLEMENTATION
#include "../thirdparty/tsf.h"

#define TML_IMPLEMENTATION
#include "../thirdparty/tml.h"

extern ClientData _Client;
extern InputTracking _InputTracking;
extern Custom _Custom;

static tml_message *TinyMidiLoader;

// Holds the global instance pointer
static tsf *g_TinySoundFont;
// Holds global MIDI playback state
static double g_Msec;              // current playback time
static tml_message *g_MidiMessage; // next message to be played

static float *midi_buffer;

static SDL_Window *window;
static SDL_Surface *window_surface;
static SDL_Texture *texture;
static SDL_Renderer *renderer;
static SDL_AudioStream *midi_stream;

static void platform_get_keycodes(const SDL_KeyboardEvent *e, int *code, char *ch);

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
        SDL_free(wave_buffer);
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
            SDL_free(wave_buffer);
            return;
        }
        if (!SDL_ResumeAudioStreamDevice(wave_stream)) {
            rs2_error("SDL3: ResumeAudioStreamDevice(Wave) failed: %s\n", SDL_GetError());
            SDL_free(wave_buffer);
            return;
        }
    }
    SDL_free(wave_buffer);
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

void platform_new(int width, int height) {
    int init = SDL_INIT_VIDEO;
    if (!_Client.lowmem) {
        init |= SDL_INIT_AUDIO;
    }
    if (!SDL_Init(init)) {
        rs2_error("SDL3: SDL_Init failed: %s\n", SDL_GetError());
        return;
    }
    int win_flags = 0;
    if (_Custom.resizable) {
        win_flags |= SDL_WINDOW_RESIZABLE;
    }
    window = SDL_CreateWindow("Jagex", width, height, win_flags);
    if (!window) {
        rs2_error("SDL3: window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return;
    }

    if (!_Custom.resizable) {
        window_surface = SDL_GetWindowSurface(window);
        if (!window_surface) {
            rs2_error("SDL3: window surface creation failed: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }
    } else {
        window_surface = SDL_CreateSurface(width, height, SDL_GetPixelFormatForMasks(32, 0xff0000, 0x00ff00, 0x0000ff, 0));
        if (!window_surface) {
            rs2_error("SDL2: surface creation failed: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }

        int num_renderers = SDL_GetNumRenderDrivers();
        if (num_renderers == 0) {
            rs2_error("SDL3: no renderers available!\n");
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }

        renderer = SDL_CreateRenderer(window, NULL);
        if (!renderer) {
            rs2_error("SDL3: renderer creation failed: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }

        char renderers[1024] = {0};
        const char *r_name = SDL_GetRendererName(renderer);
        for (int i = 0; i < num_renderers; i++) {
            const char *name = SDL_GetRenderDriver(i);
            if (i > 0) {
                strcat(renderers, ", ");
            }
            strcat(renderers, name);
            if (!strcmp(name, r_name)) {
                strcat(renderers, "*");
            }
        }
        rs2_log("SDL renderers: [%s]\n", renderers);

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
        if (!texture) {
            rs2_error("SDL3: texture creation failed: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }

        SDL_SetRenderLogicalPresentation(renderer, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    }
    // audio
    if (_Client.lowmem) {
        return;
    }
    // Create MIDI audio stream:
    const SDL_AudioSpec midi_spec = {SDL_AUDIO_F32, 2, 44100};

    g_TinySoundFont = tsf_load_filename("SCC1_Florestan.sf2");
    if (!g_TinySoundFont) {
        rs2_error("Could not load SoundFont\n");
    } else {
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
    const SDL_AudioSpec wave_spec = {SDL_AUDIO_U8, 1, 22050};
    wave_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &wave_spec, NULL, NULL);
    if (!wave_stream) {
        rs2_error("SDL3: OpenAudioDeviceStream(Wave) failed: %s\n", SDL_GetError());
    }
}

void platform_free(void) {
    if (!_Client.lowmem) {
        SDL_DestroyAudioStream(midi_stream);
        SDL_DestroyAudioStream(wave_stream);
        free(midi_buffer);
        tsf_close(g_TinySoundFont);
        tml_free(TinyMidiLoader);
    }
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
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
    if (midi_stream) {
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
    if (!_Custom.resizable) {
        SDL_Rect dest = {x, y, w, h};
        // SDL_BlitSurfaceScaled(surface, NULL, window_surface, &dest, SDL_SCALEMODE_LINEAR);
        // SDL_BlitSurfaceScaled(surface, NULL, window_surface, &dest, SDL_SCALEMODE_NEAREST);
        SDL_BlitSurface(surface, NULL, window_surface, &dest);
    } else {
        int *pix_write = NULL;
        int _pitch_unused = 0;
        if (!SDL_LockTexture(texture, NULL, (void **)&pix_write, &_pitch_unused) || pix_write == NULL) {
            rs2_error("SDL3: SDL_LockTexture failed: %s\n", SDL_GetError());
            return;
        }
        int row_size = w * sizeof(int);
        int *src_pixels = (int *)surface->pixels;
        for (int src_y = y; src_y < (y + h); src_y++) {
            // Compute the starting index for the destination row
            int *dest_row = &pix_write[(src_y * SCREEN_WIDTH) + x];

            // Copy a row of pixels
            memcpy(dest_row, &src_pixels[(src_y - y) * w], row_size);
        }
        SDL_UnlockTexture(texture);
        SDL_RenderTexture(renderer, texture, NULL, NULL);
    }
}

void platform_update_surface(void) {
    if (_Custom.resizable) {
        SDL_RenderPresent(renderer);
    } else {
        SDL_UpdateWindowSurface(window);
    }
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

    if (_Custom.resizable) {
        platform_blit_surface(0, 0, window_surface->w, window_surface->h, window_surface);
    }
}

void platform_fill_rect(int x, int y, int w, int h, int color) {
    SDL_Rect rect = {x, y, w, h};
    SDL_FillSurfaceRect(window_surface, &rect, color);

    if (_Custom.resizable) {
        platform_blit_surface(0, 0, window_surface->w, window_surface->h, window_surface);
    }
}

static void platform_get_keycodes(const SDL_KeyboardEvent *e, int *code, char *ch) {
    *ch = -1;

    switch (e->scancode) {
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
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
        *code = K_CONTROL;
        break;
    case SDL_SCANCODE_PAGEUP:
        *code = K_PAGE_UP;
        break;
    case SDL_SCANCODE_PAGEDOWN:
        *code = K_PAGE_DOWN;
        break;
    case SDL_SCANCODE_END:
        *code = K_END;
        break;
    case SDL_SCANCODE_HOME:
        *code = K_HOME;
        break;
    case SDL_SCANCODE_F1:
        *code = K_F1;
        break;
    case SDL_SCANCODE_F2:
        *code = K_F2;
        break;
    case SDL_SCANCODE_F3:
        *code = K_F3;
        break;
    case SDL_SCANCODE_F4:
        *code = K_F4;
        break;
    case SDL_SCANCODE_F5:
        *code = K_F5;
        break;
    case SDL_SCANCODE_F6:
        *code = K_F6;
        break;
    case SDL_SCANCODE_F7:
        *code = K_F7;
        break;
    case SDL_SCANCODE_F8:
        *code = K_F8;
        break;
    case SDL_SCANCODE_F9:
        *code = K_F9;
        break;
    case SDL_SCANCODE_F10:
        *code = K_F10;
        break;
    case SDL_SCANCODE_F11:
        *code = K_F11;
        break;
    case SDL_SCANCODE_F12:
        *code = K_F12;
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
        *ch = e->key;

        switch (e->scancode) {
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

        if (e->mod & SDL_KMOD_SHIFT) {
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
        case SDL_EVENT_QUIT:
            gameshell_destroy(c);
            break;
        case SDL_EVENT_KEY_DOWN: {
            char ch;
            int code;
            platform_get_keycodes((SDL_KeyboardEvent *)&e, &code, &ch);
            key_pressed(c->shell, code, ch);
            break;
        }
        case SDL_EVENT_KEY_UP: {
            char ch;
            int code;
            platform_get_keycodes((SDL_KeyboardEvent *)&e, &code, &ch);
            key_released(c->shell, code, ch);
            break;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            if (_Custom.resizable) {
                if (!SDL_ConvertEventToRenderCoordinates(renderer, &e)) {
                    rs2_error("SDL3: failed to translate mouse event: %s", SDL_GetError());
                    break;
                }
            }
            int x = e.motion.x;
            int y = e.motion.y;

            c->shell->idle_cycles = 0;
            c->shell->mouse_x = x;
            c->shell->mouse_y = y;

            if (_InputTracking.enabled) {
                inputtracking_mouse_moved(&_InputTracking, x, y);
            }
        } break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (_Custom.resizable) {
                if (!SDL_ConvertEventToRenderCoordinates(renderer, &e)) {
                    rs2_error("SDL3: failed to translate mouse event: %s", SDL_GetError());
                    break;
                }
            }
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
        case SDL_EVENT_MOUSE_BUTTON_UP:
            c->shell->idle_cycles = 0;
            c->shell->mouse_button = 0;

            if (_InputTracking.enabled) {
                inputtracking_mouse_released(&_InputTracking, (e.button.button & SDL_BUTTON_RMASK) != 0 ? 1 : 0);
            }
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            if (_Custom.resizable) {
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xff);
                SDL_RenderClear(renderer);
            } else {
                window_surface = SDL_GetWindowSurface(window);
                if (!window_surface) {
                    rs2_error("SDL3: failed to get window surface: %s\n", SDL_GetError());
                    SDL_DestroyWindow(window);
                    SDL_Quit();
                    return;
                }
            }
            c->redraw_background = true;
            break;
        case SDL_EVENT_WINDOW_MOUSE_ENTER:
            if (_InputTracking.enabled) {
                inputtracking_mouse_entered(&_InputTracking);
            }
            break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            if (_InputTracking.enabled) {
                inputtracking_mouse_exited(&_InputTracking);
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
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
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            c->shell->has_focus = false; // mapview applet
            if (_InputTracking.enabled) {
                inputtracking_focus_lost(&_InputTracking);
            }
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
