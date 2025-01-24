#pragma once

#include "animbase.h"

typedef struct {
    int delay;
    AnimBase *base;
    int length;
    int *groups;
    int *x;
    int *y;
    int *z;
} AnimFrame;

typedef struct {
    AnimFrame **instances;
    int count;
} AnimFrameData;

void animframe_free_global(void);
void animframe_unpack(Jagfile *models);
