#pragma once

#include "entity.h"
#include "model.h"
#include "spotanimtype.h"

typedef struct {
    Entity entity;
    SpotAnimType *type;
    int startCycle;
    int level;
    int x;
    int z;
    int y;
    int seqFrame;
    int seqCycle;
    bool seqComplete;
} SpotAnimEntity;

SpotAnimEntity *spotanimentity_new(int id, int level, int x, int z, int y, int cycle, int delay);
void spotanimentity_update(SpotAnimEntity *entity, int delta);
Model *spotanimentity_draw(SpotAnimEntity *entity, int loopCycle);
