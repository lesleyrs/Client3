#include <stdlib.h>

#include "locentity.h"
#include "platform.h"

LocEntity *locentity_new(int index, int level, int type, int x, int z, SeqType *seq, bool randomFrame) {
    LocEntity *entity = calloc(1, sizeof(LocEntity));
    entity->link = (Linkable){0};
    entity->level = level;
    entity->type = type;
    entity->x = x;
    entity->z = z;
    entity->index = index;
    entity->seq = seq;

    if (randomFrame && seq->replayoff != -1) {
        entity->seqFrame = (int)(jrand() * (double)entity->seq->frameCount);
        entity->seqCycle = (int)(jrand() * (double)entity->seq->delay[entity->seqFrame]);
    } else {
        entity->seqFrame = -1;
        entity->seqCycle = 0;
    }
    return entity;
}
