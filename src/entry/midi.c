#ifdef midi
#include <stdbool.h>
#include <stdio.h>

#include "SDL.h"
// #include "../packet.h"

#define TSF_IMPLEMENTATION
#include "../thirdparty/tsf.h"

#define TML_IMPLEMENTATION
#include "../thirdparty/tml.h"

#include "../thirdparty/bzip.h"

tml_message *TinyMidiLoader = NULL;

void set_midi(const char *name, int crc, int length, bool fade) {
    char filename[50];
    // snprintf(filename, sizeof(filename), "cache/client/jingles/%s.mid", name);
    snprintf(filename, sizeof(filename), "cache/client/songs/%s.mid", name);
    printf("%s\n", filename);
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error loading midi file");
        return;
    }

    int8_t *data = malloc(length);
    const int len = fread(data, 1, length, file);

    const int uncompressed_length = (data[0] & 0xff) << 24 | (data[1] & 0xff) << 16 | (data[2] & 0xff) << 8 | (data[3] & 0xff);
    // Packet *packet = packet_new(data, 4);
    // const int uncompressed_length = g4(packet);
    printf("len %d archive len %d orig %d\n", uncompressed_length, len, length);
    int8_t *uncompressed = malloc(uncompressed_length);
    bzip_decompress(uncompressed, data, uncompressed_length, 4);
    TinyMidiLoader = tml_load_memory(uncompressed, uncompressed_length);
    free(data);
    // packet_free(packet);
    free(uncompressed);
    fclose(file);
    return;
}

// Holds the global instance pointer
static tsf *g_TinySoundFont;

// Holds global MIDI playback state
static double g_Msec;              // current playback time
static tml_message *g_MidiMessage; // next message to be played

// Callback function called by the audio thread
static void AudioCallback(void *data, Uint8 *stream, int len) {
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

int main(int argc, char **argv) {
    // This implements a small program that you can launch without
    // parameters for a default file & soundfont, or with these arguments:
    //
    // ./example3-... <yourfile>.mid <yoursoundfont>.sf2

    // Define the desired audio output format we request
    SDL_AudioSpec OutputAudioSpec;
    OutputAudioSpec.freq = 44100;
#if SDL == 1
    OutputAudioSpec.format = AUDIO_S16SYS;
#else
    OutputAudioSpec.format = AUDIO_F32;
#endif
    OutputAudioSpec.channels = 2;
    OutputAudioSpec.samples = 4096;
    OutputAudioSpec.callback = AudioCallback;

    // Initialize the audio system
    if (SDL_AudioInit(TSF_NULL) < 0) {
        fprintf(stderr, "Could not initialize audio hardware or driver\n");
        return 1;
    }

    set_midi(argc > 1 ? argv[1] : "scape_main", 12345678, 40000, false);

    if (!TinyMidiLoader) {
        fprintf(stderr, "Could not load MIDI file %s\n", argv[1]);
        return 1;
    }

    // Set up the global MidiMessage pointer to the first MIDI message
    g_MidiMessage = TinyMidiLoader;

    // Load the SoundFont from a file
    g_TinySoundFont = tsf_load_filename(
        (argc > 2 ? argv[2] : "SCC1_Florestan.sf2"));
    if (!g_TinySoundFont) {
        fprintf(stderr, "Could not load SoundFont\n");
        return 1;
    }

    // Initialize preset on special 10th MIDI channel to use percussion sound bank (128) if available
    tsf_channel_set_bank_preset(g_TinySoundFont, 9, 128, 0);

    // Set the SoundFont rendering output mode
    tsf_set_output(g_TinySoundFont, TSF_STEREO_INTERLEAVED, OutputAudioSpec.freq, 0.0f);

    // Request the desired audio output format
    if (SDL_OpenAudio(&OutputAudioSpec, TSF_NULL) < 0) {
        fprintf(stderr, "Could not open the audio hardware or the desired audio output format\n");
        return 1;
    }

    // Start the actual audio playback here
    // The audio thread will begin to call our AudioCallback function
    SDL_PauseAudio(0);

    SDL_Event event;
    // Wait until the entire MIDI file has been played back (until the end of the linked message list is reached)
    while (g_MidiMessage) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                g_MidiMessage = NULL;
            }
        }
        SDL_Delay(100);
    }

    // We could call tsf_close(g_TinySoundFont) and tml_free(TinyMidiLoader)
    // here to free the memory and resources but we just let the OS clean up
    // because the process ends here.
    return 0;
}
#endif
