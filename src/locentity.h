#pragma once

#include <stdbool.h>

#include "datastruct/linkable.h"
#include "seqtype.h"

typedef struct {
    Linkable link;
    int level;
    int type;
    int x;
    int z;
    int index;
    SeqType *seq;
    int seqFrame;
    int seqCycle;
} LocEntity;

LocEntity *locentity_new(int index, int level, int type, int x, int z, SeqType *seq, bool randomFrame);
