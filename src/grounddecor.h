#pragma once

#include "model.h"

typedef struct {
    int y;
    int x;
    int z;
    Model *model;
    int bitset;
    int8_t info;
} GroundDecor;
