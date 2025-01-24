#pragma once

#include "datastruct/linkable.h"
#include "decor.h"
#include "grounddecor.h"
#include "groundobject.h"
#include "location.h"
#include "tileoverlay.h"
#include "tileunderlay.h"
#include "wall.h"

typedef struct Ground Ground;
struct Ground {
    Linkable link;
    int level;
    int x;
    int z;
    int occludeLevel;
    TileUnderlay *underlay;
    TileOverlay *overlay;
    Wall *wall;
    Decor *decor;
    GroundDecor *groundDecor;
    GroundObject *groundObj;
    int locCount;
    Location *locs[5];
    int locSpan[5];
    int locSpans;
    int drawLevel;
    bool visible;
    bool update;
    bool containsLocs;
    int checkLocSpans;
    int blockLocSpans;
    int inverseBlockLocSpans;
    int backWallTypes;
    Ground *bridge;
};

Ground *ground_new(int level, int x, int z);
void ground_free(Ground *ground);
