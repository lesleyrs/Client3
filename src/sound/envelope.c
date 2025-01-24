#include <stdlib.h>

#include "../packet.h"
#include "envelope.h"

void envelope_free(Envelope *env) {
    free(env->shapeDelta);
    free(env->shapePeak);
    free(env);
}

void envelope_read(Envelope *env, Packet *dat) {
    env->form = g1(dat);
    env->start = g4(dat);
    env->end = g4(dat);
    env->length = g1(dat);
    env->shapeDelta = calloc(env->length, sizeof(int));
    env->shapePeak = calloc(env->length, sizeof(int));

    for (int i = 0; i < env->length; i++) {
        env->shapeDelta[i] = g2(dat);
        env->shapePeak[i] = g2(dat);
    }
}

void envelope_reset(Envelope *env) {
    env->threshold = 0;
    env->position = 0;
    env->delta = 0;
    env->amplitude = 0;
    env->ticks = 0;
}

int envelope_evaluate(Envelope *env, int delta) {
    if (env->ticks >= env->threshold) {
        env->amplitude = env->shapePeak[env->position++] << 15;

        if (env->position >= env->length) {
            env->position = env->length - 1;
        }

        env->threshold = (int)((double)env->shapeDelta[env->position] / 65536.0 * (double)delta);
        if (env->threshold > env->ticks) {
            env->delta = ((env->shapePeak[env->position] << 15) - env->amplitude) / (env->threshold - env->ticks);
        }
    }

    env->amplitude += env->delta;
    env->ticks++;
    return (env->amplitude - env->delta) >> 15;
}
