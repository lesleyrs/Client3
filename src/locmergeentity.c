#include <stdlib.h>

#include "locmergeentity.h"

LocMergeEntity *locmergeentity_new(int plane, int layer, int x, int z, int locIndex, int angle, int shape, int lastCycle) {
    LocMergeEntity *entity = calloc(1, sizeof(LocMergeEntity));
    entity->plane = plane;
    entity->layer = layer;
    entity->x = x;
    entity->z = z;
    entity->locIndex = locIndex;
    entity->angle = angle;
    entity->shape = shape;
    entity->lastCycle = lastCycle;
    return entity;
}
