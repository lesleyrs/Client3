#pragma once

#include "envelope.h"

typedef struct {
    Envelope *frequencyBase;
    Envelope *amplitudeBase;
    Envelope *frequencyModRate;
    Envelope *frequencyModRange;
    Envelope *amplitudeModRate;
    Envelope *amplitudeModRange;
    Envelope *release;
    Envelope *attack;
    int harmonicVolume[5];
    int harmonicSemitone[5];
    int harmonicDelay[5];
    int reverbDelay;
    int reverbVolume; // = 100;
    int length;       // = 500;
    int start;
} Tone;

typedef struct {
    int *buffer;
    int *noise;
    int *sin;
    int tmpPhases[5];
    int tmpDelays[5];
    int tmpVolumes[5];
    int tmpSemitones[5];
    int tmpStarts[5];
} ToneData;

Tone *tone_new(void);
void tone_free(Tone *tone);
void tone_free_global(void);
void tone_init_global(void);
int *tone_generate(Tone *tone, int sampleCount, int length);
void tone_read(Tone *tone, Packet *dat);
