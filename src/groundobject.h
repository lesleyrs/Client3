#pragma once

#include "model.h"

typedef struct {
    int y;
    int x;
    int z;
    Model *topObj;
    Model *bottomObj;
    Model *middleObj;
    int bitset;
    int offset;
} GroundObject;
