#pragma once

#include "model.h"

typedef struct {
    int y;
    int x;
    int z;
    int type;
    int angle;
    Model *model;
    int bitset;
    int8_t info;
} Decor;
