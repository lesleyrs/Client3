#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "collisionmap.h"
#include "defines.h"
#include "world3d.h"

// name and packaging confirmed 100% in rs2/mapview applet strings
typedef struct {
    int maxTileX;
    int maxTileZ;
    int (*levelHeightmap)[104 + 1][104 + 1];
    int8_t (*levelTileFlags)[104][104];
    int8_t (*levelTileUnderlayIds)[104][104];
    int8_t (*levelTileOverlayIds)[104][104];
    int8_t (*levelTileOverlayShape)[104][104];
    int8_t (*levelTileOverlayRotation)[104][104];
    int8_t (*levelShademap)[104 + 1][104 + 1];
    int (*levelLightmap)[104 + 1];
    int *blendChroma;
    int *blendSaturation;
    int *blendLightness;
    int *blendLuminance;
    int *blendMagnitude;
    int (*levelOccludemap)[104 + 1][104 + 1];
} World;

typedef struct {
    bool lowMemory; // = true;
    int levelBuilt;
    bool fullbright;
    int randomHueOffset;
    int randomLightnessOffset;
} WorldData;

void world_init_global(void);
World *world_new(int maxTileX, int maxTileZ, int (*levelHeightmap)[104 + 1][104 + 1], int8_t (*levelTileFlags)[104][104]);
void world_free(World *world);
int perlinNoise(int x, int z);
int interpolatedNoise(int x, int z, int scale);
int interpolate(int a, int b, int x, int scale);
int smoothNoise(int x, int y);
int noise(int x, int y);
void world_add_loc(int level, int x, int z, World3D *scene, int (*levelHeightmap)[104 + 1][104 + 1], LinkList *locs, CollisionMap *collision, int locId, int shape, int rotation, int trueLevel);
void clearLandscape(World *world, int startX, int startZ, int endX, int endZ);
void world_load_ground(World *world, int originX, int originZ, int xOffset, int zOffset, int8_t *src, int src_len);
void world_load_locations(World *world, World3D *scene, LinkList *locs, CollisionMap **collision, int8_t *src, int src_len, int xOffset, int zOffset);
void world_add_loc2(World *world, int level, int x, int z, World3D *scene, LinkList *locs, CollisionMap *collision, int locId, int shape, int rotation);
void world_build(World *world, World3D *scene, CollisionMap **collision);
int world_get_drawlevel(World *world, int level, int stx, int stz);
