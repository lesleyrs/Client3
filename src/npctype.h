#pragma once

#include "datastruct/lrucache.h"
#include "jagfile.h"
#include "model.h"
#include "packet.h"

// name taken from rs3
typedef struct {
    int64_t index; // = -1L;
    char *name;
    char *desc;
    int8_t size; // = 1;
    int *models;
    int *heads;
    int readyanim;     // = -1;
    int walkanim;      // = -1;
    int walkanim_b;    // = -1;
    int walkanim_r;    // = -1;
    int walkanim_l;    // = -1;
    bool animHasAlpha; // = false;
    int *recol_s;
    int *recol_d;
    char **op;
    int resizex;  // = -1;
    int resizey;  // = -1;
    int resizez;  // = -1;
    bool minimap; // = true;
    int vislevel; // = -1;
    int resizeh;  // = 128;
    int resizev;  // = 128;

    // custom
    int models_count;
    int recol_count;
    int heads_count;
} NpcType;

typedef struct {
    int count;
    int *offsets;
    Packet *dat;
    NpcType **cache;
    int cachePos;
    LruCache *modelCache; // = new LruCache(30);
} NpcTypeData;

void npctype_unpack(Jagfile *config);
void npctype_free_global(void);
NpcType *npctype_get(int id);
Model *npctype_get_sequencedmodel(NpcType *npc, int primaryTransformId, int secondaryTransformId, int *seqMask);
Model *npctype_get_headmodel(NpcType *npc);
