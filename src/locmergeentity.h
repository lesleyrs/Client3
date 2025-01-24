#pragma once

#include "datastruct/linkable.h"
typedef struct {
    Linkable link;
    int plane;
    int layer;
    int x;
    int z;
    int locIndex;
    int angle;
    int shape;
    int lastCycle;
} LocMergeEntity;

LocMergeEntity *locmergeentity_new(int plane, int layer, int x, int z, int locIndex, int angle, int shape, int lastCycle);
