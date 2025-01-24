#pragma once

#include "../packet.h"

typedef struct {
    int length;
    int *shapeDelta;
    int *shapePeak;
    int start;
    int end;
    int form;
    int threshold;
    int position;
    int delta;
    int amplitude;
    int ticks;
} Envelope;

void envelope_free(Envelope *env);
void envelope_read(Envelope *env, Packet *dat);
void envelope_reset(Envelope *env);
int envelope_evaluate(Envelope *env, int delta);
