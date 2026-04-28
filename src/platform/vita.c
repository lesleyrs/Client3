#if defined(__vita__) && (!defined(SDL) || SDL == 0)
#include <malloc.h>
#include <string.h>
#include <errno.h>

#include <psp2/audioout.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/touch.h>

#include "../client.h"
#include "../custom.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../pixmap.h"
#include "../platform.h"
#include "../gl11.h"

#include "../thirdparty/bzip.h"
#define TSF_IMPLEMENTATION
#include "../thirdparty/tsf.h"
#define TML_IMPLEMENTATION
#include "../thirdparty/tml.h"

extern ClientData _Client;
extern InputTracking _InputTracking;
extern Custom _Custom;

#ifndef GL11
#define SCREEN_FB_SIZE (2 * 1024 * 1024) // Must be 256KB aligned
static SceUID displayblock;
static void *base; // pointer to frame buffer
static int mutex;
#endif
static int xoff = (SCREEN_FB_WIDTH - SCREEN_WIDTH) / 2;

static uint64_t systemtime_start = 0;

static SceTouchData touch[SCE_TOUCH_PORT_MAX_NUM], touch_old[SCE_TOUCH_PORT_MAX_NUM];
static SceCtrlData ctrl, ctrl_old;

#define AUDIO_PORT_SAMPLES 1024
static int g_AudioPort = -1;
#define AUDIO_THREAD_STACK (16 * 1024)
static SceUID g_AudioThread = -1;

#define WAVE_FREQ 22050
static SceUID g_WaveMutex = -1;
static bool g_WaveActive = false;
static int16_t *g_WaveSamples = NULL;
static int g_WaveSampleCount = 0;
static int g_WaveSamplePos = 0;
static int g_WaveVolume = 128;

#define MIDI_FREQ 22050
static SceUID g_MidiMutex = -1;
static bool g_MidiActive = false;
static tml_message *TinyMidiLoader = NULL;
static tsf *g_TinySoundFont = NULL;
static double g_Msec = 0.0;
static tml_message *g_MidiMessage = NULL;
static float g_MidiVolume = 1.0f;

static int16_t *decode_wave_to_s16_mono(const int8_t *src, int length, int *out_sample_count) {
    if (!src || length <= 44 || !out_sample_count) {
        return NULL;
    }

    const uint8_t *in_data = (const uint8_t *)src + 44;
    int sample_count = length - 44;
    int16_t *decoded = malloc((size_t)sample_count * sizeof(int16_t));
    if (!decoded) {
        return NULL;
    }

    for (int i = 0; i < sample_count; i++) {
        decoded[i] = (int16_t)(((int)in_data[i] - 128) << 8);
    }

    *out_sample_count = sample_count;
    return decoded;
}

static void wave_audio_init(void) {
    g_WaveMutex = sceKernelCreateMutex("wave_mutex", 0, 1, NULL);
    if (g_WaveMutex < 0) {
        return;
    }

    g_WaveActive = false;
    g_WaveSamplePos = 0;
}

static void wave_audio_shutdown(void) {
    if (g_WaveMutex >= 0) {
        sceKernelLockMutex(g_WaveMutex, 1, NULL);
    }
    g_WaveActive = false;
    if (g_WaveSamples) {
        free(g_WaveSamples);
        g_WaveSamples = NULL;
    }
    g_WaveSampleCount = 0;
    g_WaveSamplePos = 0;
    if (g_WaveMutex >= 0) {
        sceKernelUnlockMutex(g_WaveMutex, 1);
    }
    if (g_WaveMutex >= 0) {
        sceKernelDeleteMutex(g_WaveMutex);
        g_WaveMutex = -1;
    }
}

static void midi_reset_state_locked(void) {
    g_MidiMessage = NULL;
    g_Msec = 0.0;
    if (g_TinySoundFont) {
        tsf_reset(g_TinySoundFont);
        tsf_channel_set_bank_preset(g_TinySoundFont, 9, 128, 0);
        tsf_set_volume(g_TinySoundFont, g_MidiVolume);
    }
}

static int16_t clamp_s16(int sample) {
    if (sample < -32768) {
        return -32768;
    }
    if (sample > 32767) {
        return 32767;
    }
    return (int16_t)sample;
}

static int audio_thread_main(SceSize args, void *argp) {
    (void)args;
    (void)argp;

    int16_t mix[AUDIO_PORT_SAMPLES * 2];
    while (g_MidiActive) {
        memset(mix, 0, sizeof(mix));

        bool midi_locked = false;
        if (g_MidiMutex >= 0) {
            int midi_lock_result = sceKernelTryLockMutex(g_MidiMutex, 1);
            midi_locked = midi_lock_result >= 0;
        }
        if (midi_locked && g_TinySoundFont) {
            int16_t *stream = mix;
            int SampleBlock, SampleCount = AUDIO_PORT_SAMPLES;
            for (SampleBlock = TSF_RENDER_EFFECTSAMPLEBLOCK; SampleCount;
                 SampleCount -= SampleBlock, stream += (SampleBlock * 2)) {
                if (SampleBlock > SampleCount) {
                    SampleBlock = SampleCount;
                }

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

                tsf_render_short(g_TinySoundFont, stream, SampleBlock, 0);
            }
        }
        if (midi_locked) {
            sceKernelUnlockMutex(g_MidiMutex, 1);
        }

        bool wave_locked = false;
        if (g_WaveMutex >= 0) {
            int wave_lock_result = sceKernelTryLockMutex(g_WaveMutex, 1);
            wave_locked = wave_lock_result >= 0;
        }
        if (wave_locked && g_WaveSamples && g_WaveSamplePos < g_WaveSampleCount) {
            int wavevol = g_WaveVolume;
            if (wavevol < 0) {
                wavevol = 0;
            } else if (wavevol > 128) {
                wavevol = 128;
            }

            int remaining = g_WaveSampleCount - g_WaveSamplePos;
            int count = remaining > AUDIO_PORT_SAMPLES ? AUDIO_PORT_SAMPLES : remaining;
            for (int i = 0; i < count; i++) {
                int wave = ((int)g_WaveSamples[g_WaveSamplePos + i] * wavevol) / 128;
                int left = (int)mix[i * 2] + wave;
                int right = (int)mix[i * 2 + 1] + wave;
                mix[i * 2] = clamp_s16(left);
                mix[i * 2 + 1] = clamp_s16(right);
            }

            g_WaveSamplePos += count;
            if (g_WaveSamplePos >= g_WaveSampleCount) {
                free(g_WaveSamples);
                g_WaveSamples = NULL;
                g_WaveSampleCount = 0;
                g_WaveSamplePos = 0;
                g_WaveActive = false;
            }
        }
        if (wave_locked) {
            sceKernelUnlockMutex(g_WaveMutex, 1);
        }

        if (g_AudioPort >= 0) {
            sceAudioOutOutput(g_AudioPort, mix);
        } else {
            sceKernelDelayThread(4000);
        }
    }

    if (g_AudioPort >= 0) {
        sceAudioOutReleasePort(g_AudioPort);
        g_AudioPort = -1;
    }

    sceKernelExitDeleteThread(0);
    return 0;
}

static void midi_audio_init(void) {
    g_MidiMutex = sceKernelCreateMutex("midi_mutex", 0, 1, NULL);
    if (g_MidiMutex < 0) {
        return;
    }

    if (!_Client.lowmem) {
        g_TinySoundFont = tsf_load_filename("SCC1_Florestan.sf2");
        if (!g_TinySoundFont) {
            g_TinySoundFont = tsf_load_filename("rom/SCC1_Florestan.sf2");
        }

        if (!g_TinySoundFont) {
            rs2_error("Could not load SoundFont\n");
            return;
        } else {
            tsf_set_output(g_TinySoundFont, TSF_STEREO_INTERLEAVED, MIDI_FREQ, 0.0f);
            tsf_set_volume(g_TinySoundFont, g_MidiVolume);
            tsf_channel_set_bank_preset(g_TinySoundFont, 9, 128, 0);
        }
    }

    g_AudioPort = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, AUDIO_PORT_SAMPLES, MIDI_FREQ, SCE_AUDIO_OUT_MODE_STEREO);
    if (g_AudioPort >= 0) {
        int vols[2] = {SCE_AUDIO_OUT_MAX_VOL, SCE_AUDIO_OUT_MAX_VOL};
        sceAudioOutSetVolume(g_AudioPort, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vols);
    } else {
        rs2_error("Could not open the audio hardware\n");
        return;
    }

    g_MidiActive = true;
    g_AudioThread = sceKernelCreateThread("audio", audio_thread_main, 0x10000100, AUDIO_THREAD_STACK, 0, SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT, NULL);
    int thread_start_result = g_AudioThread >= 0 ? sceKernelStartThread(g_AudioThread, 0, NULL) : -1;
    if (g_AudioThread < 0 || thread_start_result < 0) {
        g_MidiActive = false;
        if (g_AudioThread >= 0) {
            sceKernelDeleteThread(g_AudioThread);
            g_AudioThread = -1;
        }
        if (g_AudioPort >= 0) {
            sceAudioOutReleasePort(g_AudioPort);
            g_AudioPort = -1;
        }
        if (g_TinySoundFont) {
            tsf_close(g_TinySoundFont);
            g_TinySoundFont = NULL;
        }
        sceKernelDeleteMutex(g_MidiMutex);
        g_MidiMutex = -1;
    }
}

static void midi_audio_shutdown(void) {
    if (g_MidiMutex >= 0) {
        sceKernelLockMutex(g_MidiMutex, 1, NULL);
    }
    g_MidiActive = false;
    if (TinyMidiLoader) {
        tml_free(TinyMidiLoader);
        TinyMidiLoader = NULL;
    }
    g_MidiMessage = NULL;
    g_Msec = 0.0;
    if (g_MidiMutex >= 0) {
        sceKernelUnlockMutex(g_MidiMutex, 1);
    }

    if (g_AudioThread >= 0) {
        sceKernelWaitThreadEnd(g_AudioThread, NULL, NULL);
        g_AudioThread = -1;
    }
    if (g_TinySoundFont) {
        tsf_close(g_TinySoundFont);
        g_TinySoundFont = NULL;
    }
    if (g_AudioPort >= 0) {
        sceAudioOutReleasePort(g_AudioPort);
        g_AudioPort = -1;
    }
    if (g_MidiMutex >= 0) {
        sceKernelDeleteMutex(g_MidiMutex);
        g_MidiMutex = -1;
    }
}

static void midi_set_loader(tml_message *loader) {
    if (!loader || g_MidiMutex < 0) {
        if (loader) {
            tml_free(loader);
        }
        return;
    }

    sceKernelLockMutex(g_MidiMutex, 1, NULL);
    if (TinyMidiLoader) {
        tml_free(TinyMidiLoader);
    }
    TinyMidiLoader = loader;
    g_MidiMessage = loader;
    g_Msec = 0.0;
    if (g_TinySoundFont) {
        tsf_reset(g_TinySoundFont);
        tsf_channel_set_bank_preset(g_TinySoundFont, 9, 128, 0);
        tsf_set_volume(g_TinySoundFont, g_MidiVolume);
    }
    sceKernelUnlockMutex(g_MidiMutex, 1);
}

static void audio_init(void) {
    wave_audio_init();
    midi_audio_init();
}

static void audio_shutdown(void) {
    midi_audio_shutdown();
    wave_audio_shutdown();
}

bool platform_init(void) {
    return true;
}

void platform_new(GameShell *shell) {
    (void)shell;

    systemtime_start = sceKernelGetSystemTimeWide();

#ifdef GL11
    vglInit(0x800000);
    vglWaitVblankStart(GL_TRUE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glViewport(0, 0, SCREEN_FB_WIDTH, SCREEN_FB_HEIGHT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, SCREEN_FB_WIDTH, SCREEN_FB_HEIGHT, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
#else
    mutex = sceKernelCreateMutex("fb_mutex", 0, 0, NULL);
    displayblock = sceKernelAllocMemBlock("display", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, SCREEN_FB_SIZE, NULL);
    if (displayblock < 0)
        return;
    sceKernelGetMemBlockBase(displayblock, (void **)&base);
    SceDisplayFrameBuf frame = {sizeof(frame), base, (SCREEN_FB_WIDTH), 0, SCREEN_FB_WIDTH, SCREEN_FB_HEIGHT};
    sceDisplaySetFrameBuf(&frame, SCE_DISPLAY_SETBUF_NEXTFRAME);
#endif

    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    // sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);

    // sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITAL);
    audio_init();
}

void platform_free(void) {
    audio_shutdown();
#ifndef GL11
    sceKernelDeleteMutex(mutex);
    sceDisplaySetFrameBuf(NULL, SCE_DISPLAY_SETBUF_IMMEDIATE);
    sceKernelFreeMemBlock(displayblock);
#endif
}

void platform_set_wave_volume(int wavevol) {
    if (g_WaveMutex >= 0) {
        sceKernelLockMutex(g_WaveMutex, 1, NULL);
    }
    g_WaveVolume = wavevol;
    if (g_WaveMutex >= 0) {
        sceKernelUnlockMutex(g_WaveMutex, 1);
    }
}

void platform_play_wave(int8_t *src, int length) {
    if (!src || length <= 0 || g_WaveMutex < 0) {
        return;
    }

    bool can_queue = false;
    sceKernelLockMutex(g_WaveMutex, 1, NULL);
    if (!g_WaveActive) {
        g_WaveActive = true;
        can_queue = true;
    }
    sceKernelUnlockMutex(g_WaveMutex, 1);
    if (!can_queue) {
        return;
    }

    int sample_count = 0;
    int16_t *decoded = decode_wave_to_s16_mono(src, length, &sample_count);
    if (!decoded || sample_count <= 0) {
        if (decoded) {
            free(decoded);
        }
        sceKernelLockMutex(g_WaveMutex, 1, NULL);
        g_WaveActive = false;
        sceKernelUnlockMutex(g_WaveMutex, 1);
        return;
    }

    sceKernelLockMutex(g_WaveMutex, 1, NULL);
    if (!g_WaveSamples) {
        g_WaveSamples = decoded;
        g_WaveSampleCount = sample_count;
        g_WaveSamplePos = 0;
        sceKernelUnlockMutex(g_WaveMutex, 1);
    } else {
        g_WaveActive = false;
        sceKernelUnlockMutex(g_WaveMutex, 1);
        free(decoded);
    }
}

void platform_set_midi_volume(float midivol) {
    if (_Client.lowmem || g_MidiMutex < 0) {
        return;
    }

    sceKernelLockMutex(g_MidiMutex, 1, NULL);
    g_MidiVolume = midivol;
    if (g_TinySoundFont) {
        tsf_set_volume(g_TinySoundFont, g_MidiVolume);
    }
    sceKernelUnlockMutex(g_MidiMutex, 1);
}

void platform_set_jingle(int8_t *src, int len) {
    tml_message *loader = tml_load_memory(src, len);
    free(src);

    if (!loader) {
        return;
    }
    midi_set_loader(loader);
}

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

    tml_message *loader = tml_load_memory(uncompressed, uncompressed_length);
    packet_free(packet);
    free(uncompressed);

    if (!loader) {
        return;
    }
    midi_set_loader(loader);
}

void platform_stop_midi(void) {
    if (_Client.lowmem || g_MidiMutex < 0) {
        return;
    }

    sceKernelLockMutex(g_MidiMutex, 1, NULL);
    midi_reset_state_locked();
    sceKernelUnlockMutex(g_MidiMutex, 1);
}

void platform_poll_events(Client *c) {
    static bool right_click = false;

    static SceTouchPanelInfo info;
    sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &info);

    memcpy(touch_old, touch, sizeof(touch_old));

    for (int port = 0; port < 1 /* SCE_TOUCH_PORT_MAX_NUM */; port++) {
        sceTouchPeek(port, &touch[port], 1);
        for (int i = 0; i < 1 /* SCE_TOUCH_MAX_REPORT */; i++) {
            static bool touch_down = false;
            if (touch[port].reportNum > 0) {
                int x = touch[port].report[i].x * SCREEN_FB_WIDTH / info.maxAaX - xoff;
                int y = touch[port].report[i].y * SCREEN_FB_HEIGHT / info.maxAaY;

                c->shell->idle_cycles = 0;
                c->shell->mouse_x = x;
                c->shell->mouse_y = y;

                if (_InputTracking.enabled) {
                    inputtracking_mouse_moved(&_InputTracking, x, y);
                }

                if (!touch_down) {
                    touch_down = true;
                    update_touch = true;

                    last_touch_x = x;
                    last_touch_y = y;

                    if (insideMobileInputArea(c)) {
                        // SDL_StartTextInput();
                    }

                    if (right_click) {
                        last_touch_button = 2;
                        c->shell->mouse_button = 2;
                    } else {
                        last_touch_button = 1;
                        c->shell->mouse_button = 1;
                    }

                    if (_InputTracking.enabled) {
                        inputtracking_mouse_pressed(&_InputTracking, x, y, right_click ? 1 : 0);
                    }
                }
            } else {
                if (touch_down) {
                    touch_down = false;

                    c->shell->idle_cycles = 0;
                    c->shell->mouse_button = 0;

                    if (_InputTracking.enabled) {
                        inputtracking_mouse_released(&_InputTracking, right_click ? 1 : 0);
                    }
                }
            }
        }
    }

    ctrl_old = ctrl;
    sceCtrlPeekBufferPositive(0, &ctrl, 1);

    int pressed = ctrl.buttons & ~ctrl_old.buttons;
    int released = ~ctrl.buttons & ctrl_old.buttons;

    if (pressed & SCE_CTRL_TRIANGLE) {
        key_pressed(c->shell, K_CONTROL, -1);
    }

    if (pressed & SCE_CTRL_CIRCLE) {
    }

    if (pressed & SCE_CTRL_CROSS) {
        right_click = true;
    }

    if (pressed & SCE_CTRL_SQUARE) {
    }

    if (pressed & SCE_CTRL_DOWN) {
        key_pressed(c->shell, K_DOWN, -1);
    }

    if (pressed & SCE_CTRL_LEFT) {
        key_pressed(c->shell, K_LEFT, -1);
    }

    if (pressed & SCE_CTRL_UP) {
        key_pressed(c->shell, K_UP, -1);
    }

    if (pressed & SCE_CTRL_RIGHT) {
        key_pressed(c->shell, K_RIGHT, -1);
    }

    if (pressed & SCE_CTRL_SELECT) {
        _Custom.show_performance = !_Custom.show_performance;
    }
    if (pressed & SCE_CTRL_START) {
    }
    if (pressed & SCE_CTRL_L1) {
    }
    if (pressed & SCE_CTRL_R1) {
    }

    if (released & SCE_CTRL_TRIANGLE) {
        key_released(c->shell, K_CONTROL, -1);
    }

    if (released & SCE_CTRL_CIRCLE) {
    }

    if (released & SCE_CTRL_CROSS) {
        right_click = false;
    }

    if (released & SCE_CTRL_SQUARE) {
    }

    if (released & SCE_CTRL_DOWN) {
        key_released(c->shell, K_DOWN, -1);
    }

    if (released & SCE_CTRL_LEFT) {
        key_released(c->shell, K_LEFT, -1);
    }

    if (released & SCE_CTRL_UP) {
        key_released(c->shell, K_UP, -1);
    }

    if (released & SCE_CTRL_RIGHT) {
        key_released(c->shell, K_RIGHT, -1);
    }

    if (released & SCE_CTRL_SELECT) {
    }
    if (released & SCE_CTRL_START) {
    }
    if (released & SCE_CTRL_L1) {
    }
    if (released & SCE_CTRL_R1) {
    }

    // rs2_log("\e[m Stick:[%3i:%3i][%3i:%3i]\r", ctrl.lx,ctrl.ly, ctrl.rx,ctrl.ry);
}

void platform_blit_surface(Surface *surface, int x, int y) {
#ifdef GL11
    (void)surface, (void)x, (void)y;
#else
    x += xoff;

    sceKernelLockMutex(mutex, 1, NULL);
    platform_set_pixels(base, surface, x, y, true);
    sceKernelUnlockMutex(mutex, 1);
#endif
}

void platform_update_surface(void) {
#ifdef GL11
    vglSwapBuffers(GL_FALSE);
#endif
}

uint64_t rs2_now(void) {
    return (sceKernelGetSystemTimeWide() - systemtime_start) / 1000; // fixes int to float conversions starting at 0
}

void rs2_sleep(int ms) {
    sceKernelDelayThreadCB(ms * 1000);
}
#endif
