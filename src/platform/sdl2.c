#if SDL == 2
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

#include "../thirdparty/bzip.h"
#define TSF_IMPLEMENTATION
#include "../thirdparty/tsf.h"

#define TML_IMPLEMENTATION
#include "../thirdparty/tml.h"

extern ClientData _Client;
extern InputTracking _InputTracking;
extern Custom _Custom;

#ifdef __vita__
static SDL_Joystick *joystick;
#endif
static bool right_touch = false;

static tml_message *TinyMidiLoader;

// Holds the global instance pointer
static tsf *g_TinySoundFont;
// Holds global MIDI playback state
static double g_Msec;              // current playback time
static tml_message *g_MidiMessage; // next message to be played

static SDL_Window *window;
static SDL_Surface *window_surface;
static SDL_Texture *texture;
static SDL_Renderer *renderer;

static void platform_get_keycodes(SDL_Keysym *keysym, int *code, char *ch);

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
static SDL_AudioDeviceID device;
static int g_wavevol = 128;

void platform_set_wave_volume(int wavevol) {
    g_wavevol = wavevol;
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
    if (g_wavevol != 128) {
        for (uint32_t i = 0; i < wavLength; i++) {
            wavBuffer[i] = (wavBuffer[i] - 128) * g_wavevol / 128 + 128;
        }
    }

    // TODO rm
    // rs2_log("wav %i %i %i\n", wavSpec.freq, wavSpec.samples, wavSpec.format);
    // TODO: custom option for having multiple audio devices play sounds at same time (inauthentic) web already does it
    if (SDL_GetQueuedAudioSize(device) == 0) {
        SDL_QueueAudio(device, wavBuffer, wavLength);
        SDL_PauseAudioDevice(device, 0);
    }
    SDL_FreeWAV(wavBuffer);
}
#endif /* not EMSCRIPTEN */

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

bool platform_init(void) {
    return true;
}

void platform_new(int width, int height) {
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    if (_Custom.resizable) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); // Linear scaling
    }
#ifdef ANDROID
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft");
    // SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight"); NOTE: maybe use this instead based on feedback
#endif
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    // SDL_SetHint(SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4, "1"); // OSRS desktop client always had this, not sure about before?

    int init = SDL_INIT_VIDEO;
    if (!_Client.lowmem) {
        init |= SDL_INIT_AUDIO;
    }
#ifdef __vita__
    init |= SDL_INIT_JOYSTICK;
#endif
    if (SDL_Init(init) < 0) {
        rs2_error("SDL_Init failed: %s\n", SDL_GetError());
        return;
    }
#ifdef __vita__
    SDL_JoystickEventState(SDL_ENABLE);
    joystick = SDL_JoystickOpen(0);
#endif

    int window_flags = SDL_WINDOW_SHOWN;
    if (_Custom.resizable) {
        window_flags |= SDL_WINDOW_RESIZABLE;
    }
    window = SDL_CreateWindow("Jagex", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, window_flags);
    if (!window) {
        rs2_error("SDL2: window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return;
    }

#ifdef ANDROID
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
#endif

    if (!_Custom.resizable) {
        window_surface = SDL_GetWindowSurface(window);
        if (!window_surface) {
            rs2_error("SDL2: window surface creation failed: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }
    } else {
        window_surface = SDL_CreateRGBSurface(0, width, height, 32, 0xff0000, 0x00ff00, 0x0000ff, 0);
        if (!window_surface) {
            rs2_error("SDL2: surface creation failed: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }

        int num_renderers = SDL_GetNumRenderDrivers();
        if (num_renderers == 0) {
            rs2_error("SDL2: no renderers available\n");
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
            if (!renderer) {
                rs2_error("SDL2: renderer creation failed: %s\n", SDL_GetError());
                SDL_DestroyWindow(window);
                SDL_Quit();
                return;
            }
            rs2_log("SDL2: software renderer in use\n");
        }
        SDL_RendererInfo active_info = {0};
        SDL_GetRendererInfo(renderer, &active_info);

        SDL_RendererInfo current_info = {0};
        char renderers[1024] = {0};
        for (int i = 0; i < num_renderers; i++) {
            SDL_GetRenderDriverInfo(i, &current_info);
            if (i > 0) {
                strcat(renderers, ", ");
            }
            strcat(renderers, current_info.name);
            if (!strcmp(active_info.name, current_info.name)) {
                strcat(renderers, "*");
            }
        }
        rs2_log("SDL2: available renderers: [%s]\n", renderers);

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
        if (!texture) {
            rs2_error("SDL2: texture creation failed: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }

        SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    }

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

#ifndef __EMSCRIPTEN__
    SDL_AudioSpec wavSpec;
    wavSpec.freq = 22050;
    wavSpec.format = AUDIO_U8;
    wavSpec.channels = 1;
    wavSpec.samples = 4096;
    wavSpec.callback = NULL;

    device = SDL_OpenAudioDevice(NULL, 0, &wavSpec, NULL, 0);
    if (!device) {
        rs2_error("SDL_OpenAudioDevice(): %s\n", SDL_GetError());
    }
#endif
}

void platform_free(void) {
#ifdef __vita__
    SDL_JoystickClose(0);
#endif
    if (_Custom.resizable) {
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    tsf_close(g_TinySoundFont);
    tml_free(TinyMidiLoader);
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
#ifdef ANDROID
    SDL_RWops *file = SDL_RWFromFile(filename, "rb");
#else
    FILE *file = fopen(filename, "rb");
#endif
    if (!file) {
        rs2_error("Error loading midi file %s: %s (NOTE: authentic if empty?)\n", filename, strerror(errno));
        return;
    }

    int8_t *data = malloc(len);
#ifdef ANDROID
    const size_t data_len = SDL_RWread(file, data, 1, len);
#else
    const size_t data_len = fread(data, 1, len, file);
#endif
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
#ifdef ANDROID
    SDL_RWclose(file);
#else
    fclose(file);
#endif
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
    if (!_Custom.resizable) {
        SDL_Rect dest = {x, y, w, h};
        // SDL_BlitScaled(surface, NULL, window_surface, &dest);
        SDL_BlitSurface(surface, NULL, window_surface, &dest);
    } else {
        // Lock the texture (texture) so that we may write directly to the pixels:
        int *pix_write = NULL;
        int _pitch_unused = 0;
        if (SDL_LockTexture(texture, NULL, (void **)&pix_write, &_pitch_unused) < 0) {
            rs2_error("SDL2: SDL_LockTexture failed: %s\n", SDL_GetError());
            return;
        }
        int row_size = w * sizeof(int);
        int *src_pixels = (int *)surface->pixels;
        for (int src_y = y; src_y < (y + h); src_y++) {
            // Calculate offset in texture to write a single row of pixels
            int *row = &pix_write[(src_y * SCREEN_WIDTH) + x];
            // Copy a single row of pixels
            memcpy(row, &src_pixels[(src_y - y) * w], row_size);
        }
        // Unlock the texture so that it may be used elsewhere
        SDL_UnlockTexture(texture);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
    }
}

void platform_update_surface(void) {
    if (!_Custom.resizable) {
        SDL_UpdateWindowSurface(window);
    } else {
        SDL_RenderPresent(renderer);
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
    SDL_FillRect(window_surface, &rect, color);

    if (_Custom.resizable) {
        platform_blit_surface(0, 0, window_surface->w, window_surface->h, window_surface);
    }
}

static void platform_get_keycodes(SDL_Keysym *keysym, int *code, char *ch) {
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
// TODO: apply touch to other consoles/mobile if needed
#ifdef __vita__
        case SDL_JOYAXISMOTION: {
            // rs2_log("axis %d value %d\n", e.jaxis.axis, e.jaxis.value);
        } break;
        case SDL_JOYBUTTONDOWN: {
            switch (e.jbutton.button) {
            case 0: // Triangle
                key_pressed(c->shell, K_CONTROL, -1);
                break;
            case 1: // Circle
                break;
            case 2: // Cross
                right_touch = true;
                break;
            case 3: // Square
                break;
            case 4: // L1
                break;
            case 5: // R1
                break;
            case 6: // Down
                key_pressed(c->shell, K_DOWN, -1);
                break;
            case 7: // Left
                key_pressed(c->shell, K_LEFT, -1);
                break;
            case 8: // Up
                key_pressed(c->shell, K_UP, -1);
                break;
            case 9: // Right
                key_pressed(c->shell, K_RIGHT, -1);
                break;
            case 10: // Select
                break;
            case 11: // Start
                break;
                // NOTE unused PS TV mode
                // case 12: // L2
                //     break;
                // case 13: // R2
                //     break;
                // case 14: // L3
                //     break;
                // case 15: // R3
                //     break;
            }
            break;
        } break;
        case SDL_JOYBUTTONUP: {
            switch (e.jbutton.button) {
            case 0: // Triangle
                key_released(c->shell, K_CONTROL, -1);
                break;
            case 1: // Circle
                break;
            case 2: // Cross
                right_touch = false;
                break;
            case 3: // Square
                break;
            case 4: // L1
                break;
            case 5: // R1
                break;
            case 6: // Down
                key_released(c->shell, K_DOWN, -1);
                break;
            case 7: // Left
                key_released(c->shell, K_LEFT, -1);
                break;
            case 8: // Up
                key_released(c->shell, K_UP, -1);
                break;
            case 9: // Right
                key_released(c->shell, K_RIGHT, -1);
                break;
            case 10: // Select
                break;
            case 11: // Start
                break;
                // NOTE unused PS TV mode
                // case 12: // L2
                //     break;
                // case 13: // R2
                //     break;
                // case 14: // L3
                //     break;
                // case 15: // R3
                //     break;
            }
            break;
        } break;
#endif
        case SDL_FINGERMOTION: {
            float x = e.tfinger.x * SCREEN_FB_WIDTH;
            float y = e.tfinger.y * SCREEN_FB_HEIGHT;

            c->shell->idle_cycles = 0;
            c->shell->mouse_x = x;
            c->shell->mouse_y = y;

            if (_InputTracking.enabled) {
                inputtracking_mouse_moved(&_InputTracking, x, y);
            }
        } break;
        case SDL_FINGERDOWN: {
            float x = e.tfinger.x * SCREEN_FB_WIDTH;
            float y = e.tfinger.y * SCREEN_FB_HEIGHT;

            c->shell->idle_cycles = 0;
            // NOTE: set mouse pos here again due to no mouse movement
            c->shell->mouse_x = x;
            c->shell->mouse_y = y;

            c->shell->mouse_click_x = x;
            c->shell->mouse_click_y = y;

            if (insideMobileInputArea(c)) {
                SDL_StartTextInput();
            }

            if (right_touch) {
                c->shell->mouse_click_button = 2;
                c->shell->mouse_button = 2;
            } else {
                c->shell->mouse_click_button = 1;
                c->shell->mouse_button = 1;
            }

            if (_InputTracking.enabled) {
                inputtracking_mouse_pressed(&_InputTracking, x, y, right_touch ? 1 : 0);
            }
        } break;
        case SDL_FINGERUP: {
            c->shell->idle_cycles = 0;
            c->shell->mouse_button = 0;

            if (_InputTracking.enabled) {
                inputtracking_mouse_released(&_InputTracking, right_touch ? 1 : 0);
            }
            break;
        } break;
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
        case SDL_WINDOWEVENT:
            switch (e.window.event) {
            case SDL_WINDOWEVENT_RESIZED:
                if (!_Custom.resizable) {
                    window_surface = SDL_GetWindowSurface(window);
                    if (!window_surface) {
                        rs2_error("Failed to get window surface: %s\n", SDL_GetError());
                        SDL_DestroyWindow(window);
                        SDL_Quit();
                        return;
                    }
                } else {
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xff);
                    SDL_RenderClear(renderer);
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

uint64_t get_ticks(void) {
    return SDL_GetTicks64();
}

void delay_ticks(int ticks) {
    SDL_Delay(ticks);
}
#endif
