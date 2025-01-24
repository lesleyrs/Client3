#pragma once

// shapes
#define WALL_STRAIGHT 0
#define WALL_DIAGONALCORNER 1
#define WALL_L 2
#define WALL_SQUARECORNER 3
#define WALL_DIAGONAL 9
#define WALLDECOR_STRAIGHT_NOOFFSET 4
#define WALLDECOR_STRAIGHT_OFFSET 5
#define WALLDECOR_DIAGONAL_OFFSET 6
#define WALLDECOR_DIAGONAL_NOOFFSET 7
#define WALLDECOR_DIAGONAL_BOTH 8
#define CENTREPIECE_STRAIGHT 10
#define CENTREPIECE_DIAGONAL 11
#define GROUNDDECOR 22
#define ROOF_STRAIGHT 12
#define ROOF_DIAGONAL_WITH_ROOFEDGE 13
#define ROOF_DIAGONAL 14
#define ROOF_L_CONCAVE 15
#define ROOF_L_CONVEX 16
#define ROOF_FLAT 17
#define ROOFEDGE_STRAIGHT 18
#define ROOFEDGE_DIAGONALCORNER 19
#define ROOFEDGE_L 20
#define ROOFEDGE_SQUARECORNER 21

#include <stdbool.h>

#include "datastruct/lrucache.h"
#include "jagfile.h"
#include "model.h"
#include "packet.h"

// name taken from rs3
typedef struct {
    int index; // = -1
    int *models;
    int *shapes;
    char *name;
    char *desc;
    int *recol_s;
    int *recol_d;
    int width;
    int length;
    bool blockwalk;
    bool blockrange;
    bool active;
    bool hillskew;
    bool sharelight;
    bool occlude;
    int anim;
    int wallwidth;
    int8_t ambient;
    int8_t contrast;
    char **op;
    bool animHasAlpha;
    int mapfunction;
    int mapscene;
    bool mirror;
    bool shadow;
    int resizex;
    int resizey;
    int resizez;
    int offsetx;
    int offsety;
    int offsetz;
    int forceapproach;
    bool forcedecor;

    // custom
    int shapes_and_models_count;
    int recol_count;
} LocType;

typedef struct {
    LruCache *modelCacheStatic;  // = new LruCache(500);
    LruCache *modelCacheDynamic; // = new LruCache(30);
    bool reset;
    int count;
    int *offsets;
    Packet *dat;
    LocType **cache;
    int cachePos;
} LocTypeData;

void loctype_unpack(Jagfile *config);
void loctype_free_global(void);
LocType *loctype_get(int id);
void loctype_reset(LocType *loc);
Model *loctype_get_model(LocType *loc, int shape, int rotation, int heightmapSW, int heightmapSE, int heightmapNE, int heightmapNW, int transformId);
