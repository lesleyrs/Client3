#pragma once

#include "datastruct/lrucache.h"
#include "model.h"
#include "packet.h"
#include "pathingentity.h"

typedef struct {
    PathingEntity pathing_entity;
    char *name;
    bool visible; // = false;
    int gender;
    int headicons;
    int appearances[12];
    int colors[5];
    int combatLevel;
    int64_t appearanceHashcode;
    int y;
    int locStartCycle;
    int locStopCycle;
    int locOffsetX;
    int locOffsetY;
    int locOffsetZ;
    Model *locModel;
    int minTileX;
    int minTileZ;
    int maxTileX;
    int maxTileZ;
    bool lowmem; // = false;
} PlayerEntity;

typedef struct {
    LruCache *modelCache; // = new LruCache(200);
} PlayerEntityData;

PlayerEntity *playerentity_new(void);
void playerentity_init_global(void);
void playerentity_free_global(void);
void playerentity_read(PlayerEntity *entity, Packet *buf);
Model *playerentity_draw(PlayerEntity *entity, int loopCycle);
Model *playerentity_get_sequencedmodel(PlayerEntity *entity);
Model *playerentity_get_headmodel(PlayerEntity *entity);
bool playerentity_is_visible(PlayerEntity *entity);
