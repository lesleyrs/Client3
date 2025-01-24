#pragma once

#include "datastruct/lrucache.h"
#include "entity.h"
#include "model.h"
#include "seqtype.h"

// name derived from other types + spotanim.dat (it's been renamed in NXT)
typedef struct {
    Entity *entity;
    int index;
    int model;
    int anim; // = -1;
    SeqType *seq;
    bool animHasAlpha; // = false;
    int recol_s[6];
    int recol_d[6];
    int resizeh; // = 128;
    int resizev; // = 128;
    int orientation;
    int ambient;
    int contrast;
} SpotAnimType;

typedef struct {
    int count;
    SpotAnimType **instances;
    LruCache *modelCache; // = new LruCache(30);
} SpotAnimTypeData;

void spotanimtype_unpack(Jagfile *config);
void spotanimtype_free_global(void);
Model *spotanimtype_get_model(SpotAnimType *spotanim);
