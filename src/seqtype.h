#pragma once

#include "jagfile.h"

// name taken from rs3
typedef struct {
    int frameCount;
    int *frames;
    int *iframes;
    int *delay;
    int replayoff; // = -1;
    int *walkmerge;
    bool stretches;  // = false;
    int priority;    // = 5;
    int righthand;   // = -1;
    int lefthand;    // = -1;
    int replaycount; // = 99;
} SeqType;

typedef struct {
    int count;
    SeqType **instances;
} SeqTypeData;

void seqtype_free_global(void);
void seqtype_unpack(Jagfile *config);
