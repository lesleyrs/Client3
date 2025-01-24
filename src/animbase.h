#pragma once

#include "jagfile.h"

#define OP_BASE 0
#define OP_TRANSLATE 1
#define OP_ROTATE 2
#define OP_SCALE 3
#define OP_ALPHA 5

typedef struct {
    int length;
    int *types;
    int **labels;
    int *labels_count;
} AnimBase;

typedef struct {
    AnimBase **instances;
    int count;
} AnimBaseData;

void animbase_free_global(void);
void animbase_unpack(Jagfile *models);
