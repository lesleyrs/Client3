#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include "../platform.h"
#include "tone.h"

static int generate(int amplitude, int phase, int form);

ToneData _Tone = {0};

Tone *tone_new(void) {
    Tone *tone = calloc(1, sizeof(Tone));
    tone->reverbVolume = 100;
    tone->length = 500;
    return tone;
}

void tone_free(Tone *tone) {
    envelope_free(tone->frequencyBase);
    envelope_free(tone->amplitudeBase);
    if (tone->frequencyModRate) {
        envelope_free(tone->frequencyModRate);
        envelope_free(tone->frequencyModRange);
    }
    if (tone->amplitudeModRate) {
        envelope_free(tone->amplitudeModRate);
        envelope_free(tone->amplitudeModRange);
    }
    if (tone->release) {
        envelope_free(tone->release);
        envelope_free(tone->attack);
    }
    free(tone);
}

void tone_free_global(void) {
    free(_Tone.buffer);
    free(_Tone.noise);
    free(_Tone.sin);
}

void tone_init_global(void) {
    _Tone.noise = calloc(32768, sizeof(int));
    for (int i = 0; i < 32768; i++) {
        if (jrand() > 0.5) {
            _Tone.noise[i] = 1;
        } else {
            _Tone.noise[i] = -1;
        }
    }

    _Tone.sin = calloc(32768, sizeof(int));
    for (int i = 0; i < 32768; i++) {
        _Tone.sin[i] = (int)(sin((double)i / 5215.1903) * 16384.0);
    }

    _Tone.buffer = calloc(220500, sizeof(int)); // 10s * 22050 KHz
}

int *tone_generate(Tone *tone, int sampleCount, int length) {
    for (int sample = 0; sample < sampleCount; sample++) {
        _Tone.buffer[sample] = 0;
    }

    if (length < 10) {
        return _Tone.buffer;
    }

    double samplesPerStep = (double)sampleCount / ((double)length + 0.0);

    envelope_reset(tone->frequencyBase);
    envelope_reset(tone->amplitudeBase);

    int frequencyStart = 0;
    int frequencyDuration = 0;
    int frequencyPhase = 0;

    if (tone->frequencyModRate) {
        envelope_reset(tone->frequencyModRate);
        envelope_reset(tone->frequencyModRange);
        frequencyStart = (int)((double)(tone->frequencyModRate->end - tone->frequencyModRate->start) * 32.768 / samplesPerStep);
        frequencyDuration = (int)((double)tone->frequencyModRate->start * 32.768 / samplesPerStep);
    }

    int amplitudeStart = 0;
    int amplitudeDuration = 0;
    int amplitudePhase = 0;
    if (tone->amplitudeModRate) {
        envelope_reset(tone->amplitudeModRate);
        envelope_reset(tone->amplitudeModRange);
        amplitudeStart = (int)((double)(tone->amplitudeModRate->end - tone->amplitudeModRate->start) * 32.768 / samplesPerStep);
        amplitudeDuration = (int)((double)tone->amplitudeModRate->start * 32.768 / samplesPerStep);
    }

    for (int harmonic = 0; harmonic < 5; harmonic++) {
        if (tone->harmonicVolume[harmonic] != 0) {
            _Tone.tmpPhases[harmonic] = 0;
            _Tone.tmpDelays[harmonic] = (int)((double)tone->harmonicDelay[harmonic] * samplesPerStep);
            _Tone.tmpVolumes[harmonic] = (tone->harmonicVolume[harmonic] << 14) / 100;
            _Tone.tmpSemitones[harmonic] = (int)((double)(tone->frequencyBase->end - tone->frequencyBase->start) * 32.768 * pow(1.0057929410678534, tone->harmonicSemitone[harmonic]) / samplesPerStep);
            _Tone.tmpStarts[harmonic] = (int)((double)tone->frequencyBase->start * 32.768 / samplesPerStep);
        }
    }

    for (int sample = 0; sample < sampleCount; sample++) {
        int frequency = envelope_evaluate(tone->frequencyBase, sampleCount);
        int amplitude = envelope_evaluate(tone->amplitudeBase, sampleCount);

        if (tone->frequencyModRate) {
            int rate = envelope_evaluate(tone->frequencyModRate, sampleCount);
            int range = envelope_evaluate(tone->frequencyModRange, sampleCount);
            frequency += generate(range, frequencyPhase, tone->frequencyModRate->form) >> 1;
            frequencyPhase += (rate * frequencyStart >> 16) + frequencyDuration;
        }

        if (tone->amplitudeModRate) {
            int rate = envelope_evaluate(tone->amplitudeModRate, sampleCount);
            int range = envelope_evaluate(tone->amplitudeModRange, sampleCount);
            amplitude = amplitude * ((generate(range, amplitudePhase, tone->amplitudeModRate->form) >> 1) + 32768) >> 15;
            amplitudePhase += (rate * amplitudeStart >> 16) + amplitudeDuration;
        }

        for (int harmonic = 0; harmonic < 5; harmonic++) {
            if (tone->harmonicVolume[harmonic] != 0) {
                int position = sample + _Tone.tmpDelays[harmonic];

                if (position < sampleCount) {
                    _Tone.buffer[position] += generate(amplitude * _Tone.tmpVolumes[harmonic] >> 15, _Tone.tmpPhases[harmonic], tone->frequencyBase->form);
                    _Tone.tmpPhases[harmonic] += (frequency * _Tone.tmpSemitones[harmonic] >> 16) + _Tone.tmpStarts[harmonic];
                }
            }
        }
    }

    if (tone->release) {
        envelope_reset(tone->release);
        envelope_reset(tone->attack);

        int counter = 0;
        bool muted = true;

        for (int sample = 0; sample < sampleCount; sample++) {
            int releaseValue = envelope_evaluate(tone->release, sampleCount);
            int attackValue = envelope_evaluate(tone->attack, sampleCount);

            int threshold;
            if (muted) {
                threshold = tone->release->start + ((tone->release->end - tone->release->start) * releaseValue >> 8);
            } else {
                threshold = tone->release->start + ((tone->release->end - tone->release->start) * attackValue >> 8);
            }

            counter += 256;
            if (counter >= threshold) {
                counter = 0;
                muted = !muted;
            }

            if (muted) {
                _Tone.buffer[sample] = 0;
            }
        }
    }

    if (tone->reverbDelay > 0 && tone->reverbVolume > 0) {
        int start = (int)((double)tone->reverbDelay * samplesPerStep);

        for (int sample = start; sample < sampleCount; sample++) {
            _Tone.buffer[sample] += _Tone.buffer[sample - start] * tone->reverbVolume / 100;
        }
    }

    for (int sample = 0; sample < sampleCount; sample++) {
        if (_Tone.buffer[sample] < -32768) {
            _Tone.buffer[sample] = -32768;
        }

        if (_Tone.buffer[sample] > 32767) {
            _Tone.buffer[sample] = 32767;
        }
    }

    return _Tone.buffer;
}

static int generate(int amplitude, int phase, int form) {
    if (form == 1) {
        return (phase & 0x7fff) < 16384 ? amplitude : -amplitude;
    } else if (form == 2) {
        return _Tone.sin[phase & 0x7fff] * amplitude >> 14;
    } else if (form == 3) {
        return ((phase & 0x7fff) * amplitude >> 14) - amplitude;
    } else if (form == 4) {
        return _Tone.noise[phase / 2607 & 0x7fff] * amplitude;
    } else {
        return 0;
    }
}

void tone_read(Tone *tone, Packet *dat) {
    tone->frequencyBase = calloc(1, sizeof(Envelope));
    envelope_read(tone->frequencyBase, dat);

    tone->amplitudeBase = calloc(1, sizeof(Envelope));
    envelope_read(tone->amplitudeBase, dat);

    if (g1(dat) != 0) {
        dat->pos--;

        tone->frequencyModRate = calloc(1, sizeof(Envelope));
        envelope_read(tone->frequencyModRate, dat);

        tone->frequencyModRange = calloc(1, sizeof(Envelope));
        envelope_read(tone->frequencyModRange, dat);
    }

    if (g1(dat) != 0) {
        dat->pos--;

        tone->amplitudeModRate = calloc(1, sizeof(Envelope));
        envelope_read(tone->amplitudeModRate, dat);

        tone->amplitudeModRange = calloc(1, sizeof(Envelope));
        envelope_read(tone->amplitudeModRange, dat);
    }

    if (g1(dat) != 0) {
        dat->pos--;

        tone->release = calloc(1, sizeof(Envelope));
        envelope_read(tone->release, dat);

        tone->attack = calloc(1, sizeof(Envelope));
        envelope_read(tone->attack, dat);
    }

    for (int harmonic = 0; harmonic < 10; harmonic++) {
        int volume = gsmarts(dat);
        if (volume == 0) {
            break;
        }

        tone->harmonicVolume[harmonic] = volume;
        tone->harmonicSemitone[harmonic] = gsmart(dat);
        tone->harmonicDelay[harmonic] = gsmarts(dat);
    }

    tone->reverbDelay = gsmarts(dat);
    tone->reverbVolume = gsmarts(dat);
    tone->length = g2(dat);
    tone->start = g2(dat);
}
