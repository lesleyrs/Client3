#pragma once

#include "model.h"

typedef struct {
    int y;
    int x;
    int z;
    int typeA;
    int typeB;
    Model *modelA;
    Model *modelB;
    int bitset;
    int8_t info;
} Wall;
