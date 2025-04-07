#pragma once

#include "datastruct/lrucache.h"
#include "defines.h"
#include "jagfile.h"
#include "model.h"
#include "packet.h"
#include "pix24.h"

// name taken from rs3
typedef struct {
    int index; // = -1;
    int model;
    char name[HALF_STR]; // arbitrary length
    char desc[MAX_STR];  // arbitrary length
    int *recol_s;
    int *recol_d;
    int zoom2d;
    int xan2d;
    int yan2d;
    int zan2d;
    int xof2d;
    int yof2d;
    bool code9;
    int code10;
    bool stackable;
    int cost;
    bool members;
    char **op;
    char **iop;
    int manwear;
    int manwear2;
    int8_t manwearOffsetY;
    int womanwear;
    int womanwear2;
    int8_t womanwearOffsetY;
    int manwear3;
    int womanwear3;
    int manhead;
    int manhead2;
    int womanhead;
    int womanhead2;
    int *countobj;
    int *countco;
    int certlink;
    int certtemplate;

    int recol_count;
} ObjType;

typedef struct {
    int count;
    int *offsets;
    Packet *dat;
    ObjType **cache;
    int cachePos;
    bool membersWorld;    // = true;
    LruCache *modelCache; // = new LruCache(50);
    LruCache *iconCache;  // = new LruCache(200);
} ObjTypeData;

void objtype_unpack(Jagfile *config);
void objtype_free_global(void);
ObjType *objtype_get(int id);
Pix24 *objtype_get_icon(int id, int count);
// NOTE: custom
Pix24 *objtype_get_icon_outline(int id, int count, int outline_color);
void objtype_reset(ObjType *obj);
void objtype_to_certificate(ObjType *obj);
Model *objtype_get_interfacemodel(ObjType *obj, int count, bool use_allocator);
Model *objtype_get_wornmodel(ObjType *obj, int gender);
Model *objtype_get_headmodel(ObjType *obj, int gender);
