#pragma once

#include <stdbool.h>

#include "defines.h"
#include "ground.h"
#include "occlude.h"

#define LEVEL_COUNT 4

// name taken from rsc
typedef struct {
    int maxLevel;
    int maxTileX;
    int maxTileZ;
    int (*levelHeightmaps)[COLLISIONMAP_SIZE + 1][COLLISIONMAP_SIZE + 1];
    Ground ****levelTiles;
    int minLevel;
    int temporaryLocCount;
    Location *temporaryLocs[5000];
    int (*levelTileOcclusionCycles)[COLLISIONMAP_SIZE + 1][COLLISIONMAP_SIZE + 1];
    int mergeIndexA[10000];
    int mergeIndexB[10000];
    int tmpMergeIndex;
} World3D;

typedef struct {
    bool lowMemory; // = true;
    int tilesRemaining;
    int topLevel;
    int cycle;
    int minDrawTileX;
    int maxDrawTileX;
    int minDrawTileZ;
    int maxDrawTileZ;
    int eyeTileX;
    int eyeTileZ;
    int eyeX;
    int eyeY;
    int eyeZ;
    int sinEyePitch;
    int cosEyePitch;
    int sinEyeYaw;
    int cosEyeYaw;
    Location *locBuffer[LOCBUFFER_COUNT];
    bool takingInput;
    int mouseX;
    int mouseY;
    int clickTileX; // = -1;
    int clickTileZ; // = -1;
    int levelOccluderCount[LEVEL_COUNT];
    Occlude *levelOccluders[LEVEL_COUNT][500];
    int activeOccluderCount;
    Occlude *activeOccluders[500];
    LinkList *drawTileQueue;   // = new LinkList();
    bool (*visibilityMatrix)[32][51][51]; //[8][32][51][51];
    bool (*visibilityMap)[51];
    int viewportCenterX;
    int viewportCenterY;
    int viewportLeft;
    int viewportTop;
    int viewportRight;
    int viewportBottom;
} SceneData;

#include "datastruct/linklist.h"
#include "entity.h"
#include "ground.h"
#include "location.h"
#include "model.h"
#include "occlude.h"

void world3d_init_global(void);
void world3d_free_global(void);
World3D *world3d_new(int (*levelHeightmaps)[COLLISIONMAP_SIZE + 1][COLLISIONMAP_SIZE + 1], int maxTileZ, int maxLevel, int maxTileX);
void world3d_free(World3D *world3d, int maxTileZ, int maxLevel, int maxTileX);
void world3d_add_occluder(int level, int type, int minX, int minY, int minZ, int maxX, int maxY, int maxZ);
void world3d_init(int viewportWidth, int viewportHeight, int frustumStart, int frustumEnd, int *pitchDistance);
bool world3d_test_point(int x, int z, int y);
void world3d_reset(World3D *world3d);
void world3d_set_minlevel(World3D *world3d, int level);
void world3d_set_bridge(World3D *world3d, int stx, int stz);
void world3d_set_drawlevel(World3D *world3d, int level, int stx, int stz, int drawLevel);
void world3d_set_tile(World3D *world3d, int level, int x, int z, int shape, int angle, int textureId, int southwestY, int southeastY, int northeastY, int northwestY, int southwestColor, int southeastColor, int northeastColor, int northwestColor, int southwestColor2, int southeastColor2, int northeastColor2, int northwestColor2, int backgroundRgb, int foregroundRgb);
void world3d_add_grounddecoration(World3D *world3d, Model *model, int tileLevel, int tileX, int tileZ, int y, int bitset, int8_t info);
void world3d_add_objstack(World3D *world3d, int stx, int stz, int y, int level, int bitset, Model *topObj, Model *middleObj, Model *bottomObj);
void world3d_add_wall(World3D *world3d, int level, int tileX, int tileZ, int y, int typeA, int typeB, Model *modelA, Model *modelB, int bitset, int8_t info);
void world3d_set_walldecoration(World3D *world3d, int level, int tileX, int tileZ, int y, int offsetX, int offsetZ, int bitset, Model *model, int8_t info, int angle, int type);
bool world3d_add_loc(World3D *world3d, int level, int tileX, int tileZ, int y, Model *model, Entity *entity, int bitset, int8_t info, int width, int length, int yaw);
bool world3d_add_temporary(World3D *world3d, int level, int x, int y, int z, Model *model, Entity *entity, int bitset, int yaw, int padding, bool forwardPadding);
bool world3d_add_temporary2(World3D *world3d, int level, int x, int y, int z, int minTileX, int minTileZ, int maxTileX, int maxTileZ, Model *model, Entity *entity, int bitset, int yaw);
bool world3d_add_loc2(World3D *world3d, int x, int z, int y, int level, int tileX, int tileZ, int tileSizeX, int tileSizeZ, Model *model, Entity *entity, int bitset, int8_t info, int yaw, bool temporary);
void world3d_clear_temporarylocs(World3D *world3d);
void world3d_remove_loc2(World3D *world3d, Location *loc);
void world3d_set_locmodel(World3D *world3d, int level, int x, int z, Model *model);
void world3d_set_walldecorationoffset(World3D *world3d, int level, int x, int z, int offset);
void world3d_set_walldecorationmodel(World3D *world3d, int level, int x, int z, Model *model);
void world3d_set_grounddecorationmodel(World3D *world3d, int level, int x, int z, Model *model);
void world3d_set_wallmodel(World3D *world3d, int level, int x, int z, Model *model);
void world3d_set_wallmodels(World3D *world3d, int x, int z, int level, Model *modelA, Model *modelB);
void world3d_remove_wall(World3D *world3d, int level, int x, int z, int force);
void world3d_remove_walldecoration(World3D *world3d, int level, int x, int z);
void world3d_remove_loc(World3D *world3d, int level, int x, int z);
void world3d_remove_grounddecoration(World3D *world3d, int level, int x, int z);
void world3d_remove_objstack(World3D *world3d, int level, int x, int z);
int world3d_get_wallbitset(World3D *world3d, int level, int x, int z);
int world3d_get_walldecorationbitset(World3D *world3d, int level, int z, int x);
int world3d_get_locbitset(World3D *world3d, int level, int x, int z);
int world3d_get_grounddecorationbitset(World3D *world3d, int level, int x, int z);
int world3d_get_info(World3D *world3d, int level, int x, int z, int bitset);
void world3d_build_models(World3D *world3d, int lightAmbient, int lightAttenuation, int lightSrcX, int lightSrcY, int lightSrcZ);
void world3d_merge_grounddecorationnormals(World3D *world3d, int level, int tileX, int tileZ, Model *model);
void world3d_merge_locnormals(World3D *world3d, int level, int tileX, int tileZ, int tileSizeX, int tileSizeZ, Model *model);
void world3d_merge_normals(World3D *world3d, Model *modelA, Model *modelB, int offsetX, int offsetY, int offsetZ, bool allowFaceRemoval);
void world3d_draw_minimaptile(World3D *world3d, int level, int x, int z, int *dst, int offset, int step);
void world3d_click(int mouseX, int mouseY);
void world3d_draw(World3D *world3d, int eyeX, int eyeY, int eyeZ, int topLevel, int eyeYaw, int eyePitch, int loopCycle);
void world3d_draw_tile(World3D *world3d, Ground *next, bool checkAdjacent, int loopCycle);
void world3d_draw_tileunderlay(World3D *world3d, TileUnderlay *underlay, int level, int tileX, int tileZ, int sinEyePitch, int cosEyePitch, int sinEyeYaw, int cosEyeYaw);
void world3d_draw_tileoverlay(int tileX, int tileZ, TileOverlay *overlay, int sinEyePitch, int cosEyePitch, int sinEyeYaw, int cosEyeYaw);
int mul_lightness(int hsl, int lightness);
bool point_inside_triangle(int x, int y, int y0, int y1, int y2, int x0, int x1, int x2);
void world3d_update_activeoccluders(void);
bool world3d_tile_visible(World3D *world3d, int level, int x, int z);
bool world3d_wall_visible(World3D *world3d, int level, int x, int z, int type);
bool world3d_visible(World3D *world3d, int level, int tileX, int tileZ, int y);
bool world3d_loc_visible(World3D *world3d, int level, int minX, int maxX, int minZ, int maxZ, int y);
bool world3d_occluded(int x, int y, int z);
