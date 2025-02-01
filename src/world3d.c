#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "datastruct/linklist.h"
#include "entity.h"
#include "ground.h"
#include "location.h"
#include "model.h"
#include "occlude.h"
#include "pix2d.h"
#include "pix3d.h"
#include "platform.h"
#include "world3d.h"

SceneData _World3D = {.lowMemory = true};
extern Pix2D _Pix2D;
extern Pix3D _Pix3D;
extern TileOverlayData _TileOverlay;

const int WALL_DECORATION_INSET_X[] = {53, -53, -53, 53};
const int WALL_DECORATION_INSET_Z[] = {-53, -53, 53, 53};
const int WALL_DECORATION_OUTSET_X[] = {-45, 45, 45, -45};
const int WALL_DECORATION_OUTSET_Z[] = {45, 45, -45, -45};
const int FRONT_WALL_TYPES[] = {19, 55, 38, 155, 255, 110, 137, 205, 76};
const int DIRECTION_ALLOW_WALL_CORNER_TYPE[] = {160, 192, 80, 96, 0, 144, 80, 48, 160};
const int BACK_WALL_TYPES[] = {76, 8, 137, 4, 0, 1, 38, 2, 19};
const int WALL_CORNER_TYPE_16_BLOCK_LOC_SPANS[] = {0, 0, 2, 0, 0, 2, 1, 1, 0};
const int WALL_CORNER_TYPE_32_BLOCK_LOC_SPANS[] = {2, 0, 0, 2, 0, 0, 0, 4, 4};
const int WALL_CORNER_TYPE_64_BLOCK_LOC_SPANS[] = {0, 4, 4, 8, 0, 0, 8, 0, 0};
const int WALL_CORNER_TYPE_128_BLOCK_LOC_SPANS[] = {1, 1, 0, 0, 0, 8, 0, 0, 8};
const int TEXTURE_HSL[] = {41, 39248, 41, 4643, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 43086, 41, 41, 41, 41, 41, 41, 41, 8602, 41, 28992, 41, 41, 41, 41, 41, 5056, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 3131, 41, 41, 41};
const int MINIMAP_OVERLAY_SHAPE[13][16] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, // PLAIN_SHAPE
    {1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1}, // DIAGONAL_SHAPE
    {1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}, // LEFT_SEMI_DIAGONAL_SMALL_SHAPE
    {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1}, // RIGHT_SEMI_DIAGONAL_SMALL_SHAPE
    {0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, // LEFT_SEMI_DIAGONAL_BIG_SHAPE
    {1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1}, // RIGHT_SEMI_DIAGONAL_BIG_SHAPE
    {1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0}, // HALF_SQUARE_SHAPE
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0}, // CORNER_SMALL_SHAPE
    {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1}, // CORNER_BIG_SHAPE
    {1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}, // FAN_SMALL_SHAPE
    {0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1}, // FAN_BIG_SHAPE
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1}  // TRAPEZIUM_SHAPE
};
const int MINIMAP_OVERLAY_ROTATION[4][16] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {12, 8, 4, 0, 13, 9, 5, 1, 14, 10, 6, 2, 15, 11, 7, 3},
    {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
    {3, 7, 11, 15, 2, 6, 10, 14, 1, 5, 9, 13, 0, 4, 8, 12}};

void world3d_init_global(void) {
    _World3D.clickTileX = -1;
    _World3D.clickTileZ = -1;
    _World3D.drawTileQueue = linklist_new();

    _World3D.visibilityMatrix = calloc(8, sizeof(bool ***));
    for (int i = 0; i < 8; i++) {
        _World3D.visibilityMatrix[i] = calloc(32, sizeof(bool **));
        for (int j = 0; j < 32; j++) {
            _World3D.visibilityMatrix[i][j] = calloc(51, sizeof(bool *));
            for (int k = 0; k < 51; k++) {
                _World3D.visibilityMatrix[i][j][k] = calloc(51, sizeof(bool));
            }
        }
    }
}

World3D *world3d_new(int ***levelHeightmaps, int maxTileZ, int maxLevel, int maxTileX) {
    World3D *world3d = calloc(1, sizeof(World3D));
    world3d->maxLevel = maxLevel;
    world3d->maxTileX = maxTileX;
    world3d->maxTileZ = maxTileZ;

    world3d->levelTiles = calloc(world3d->maxLevel, sizeof(Ground ***));
    for (int i = 0; i < maxLevel; i++) {
        world3d->levelTiles[i] = calloc(world3d->maxTileX, sizeof(Ground **));
        for (int j = 0; j < maxTileX; j++) {
            world3d->levelTiles[i][j] = calloc(world3d->maxTileZ, sizeof(Ground *));
        }
    }

    world3d->levelTileOcclusionCycles = calloc(world3d->maxLevel, sizeof(int **));
    for (int i = 0; i < maxLevel; i++) {
        world3d->levelTileOcclusionCycles[i] = calloc(world3d->maxTileX + 1, sizeof(int *));
        for (int j = 0; j < maxTileX + 1; j++) {
            world3d->levelTileOcclusionCycles[i][j] = calloc(world3d->maxTileZ + 1, sizeof(int));
        }
    }
    world3d->levelHeightmaps = levelHeightmaps;
    world3d_reset(world3d);
    return world3d;
}

void world3d_free_global(void) {
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 32; j++) {
            for (int k = 0; k < 51; k++) {
                free(_World3D.visibilityMatrix[i][j][k]);
            }
            free(_World3D.visibilityMatrix[i][j]);
        }
        free(_World3D.visibilityMatrix[i]);
    }
    free(_World3D.visibilityMatrix);

    linklist_free(_World3D.drawTileQueue);

    for (int i = 0; i < LEVEL_COUNT; i++) {
        for (int j = 0; j < 500; j++) {
            free(_World3D.levelOccluders[i][j]);
        }
    }
}

void world3d_free(World3D *world3d, int maxTileZ, int maxLevel, int maxTileX) {
    for (int level = 0; level < maxLevel; level++) {
        for (int x = 0; x < maxTileX; x++) {
            for (int z = 0; z < maxTileZ; z++) {
                if (world3d->levelTiles[level][x][z]) {
                    ground_free(world3d->levelTiles[level][x][z]);
                }
            }
            free(world3d->levelTiles[level][x]);
        }
        free(world3d->levelTiles[level]);
    }
    free(world3d->levelTiles);

    for (int i = 0; i < maxLevel; i++) {
        for (int j = 0; j < maxTileX + 1; j++) {
            free(world3d->levelTileOcclusionCycles[i][j]);
        }
        free(world3d->levelTileOcclusionCycles[i]);
    }
    free(world3d->levelTileOcclusionCycles);

    free(world3d);
}

void world3d_add_occluder(int level, int type, int minX, int minY, int minZ, int maxX, int maxY, int maxZ) {
    Occlude *occluder = calloc(1, sizeof(Occlude));
    occluder->minTileX = minX / 128;
    occluder->maxTileX = maxX / 128;
    occluder->minTileZ = minZ / 128;
    occluder->maxTileZ = maxZ / 128;
    occluder->type = type;
    occluder->minX = minX;
    occluder->maxX = maxX;
    occluder->minZ = minZ;
    occluder->maxZ = maxZ;
    occluder->minY = minY;
    occluder->maxY = maxY;
    _World3D.levelOccluders[level][_World3D.levelOccluderCount[level]++] = occluder;
}

void world3d_init(int viewportWidth, int viewportHeight, int frustumStart, int frustumEnd, int *pitchDistance) {
    _World3D.viewportLeft = 0;
    _World3D.viewportTop = 0;
    _World3D.viewportRight = viewportWidth;
    _World3D.viewportBottom = viewportHeight;
    _World3D.viewportCenterX = viewportWidth / 2;
    _World3D.viewportCenterY = viewportHeight / 2;

    bool matrix[9][32][53][53];
    for (int pitch = 128; pitch <= 384; pitch += 32) {
        for (int yaw = 0; yaw < 2048; yaw += 64) {
            _World3D.sinEyePitch = _Pix3D.sin_table[pitch];
            _World3D.cosEyePitch = _Pix3D.cos_table[pitch];
            _World3D.sinEyeYaw = _Pix3D.sin_table[yaw];
            _World3D.cosEyeYaw = _Pix3D.cos_table[yaw];

            int pitchLevel = (pitch - 128) / 32;
            int yawLevel = yaw / 64;
            for (int dx = -26; dx <= 26; dx++) {
                for (int dz = -26; dz <= 26; dz++) {
                    int x = dx * 128;
                    int z = dz * 128;

                    bool visible = false;
                    for (int y = -frustumStart; y <= frustumEnd; y += 128) {
                        if (world3d_test_point(x, z, pitchDistance[pitchLevel] + y)) {
                            visible = true;
                            break;
                        }
                    }

                    matrix[pitchLevel][yawLevel][dx + 25 + 1][dz + 25 + 1] = visible;
                }
            }
        }
    }

    for (int pitchLevel = 0; pitchLevel < 8; pitchLevel++) {
        for (int yawLevel = 0; yawLevel < 32; yawLevel++) {
            for (int x = -25; x < 25; x++) {
                for (int z = -25; z < 25; z++) {

                    bool visible = false;
                    for (int dx = -1; dx <= 1; dx++) {
                        for (int dz = -1; dz <= 1; dz++) {
                            if (matrix[pitchLevel][yawLevel][x + dx + 25 + 1][z + dz + 25 + 1]) {
                                visible = true;
                                goto check_areas_done;
                            }

                            if (matrix[pitchLevel][(yawLevel + 1) % 31][x + dx + 25 + 1][z + dz + 25 + 1]) {
                                visible = true;
                                goto check_areas_done;
                            }

                            if (matrix[pitchLevel + 1][yawLevel][x + dx + 25 + 1][z + dz + 25 + 1]) {
                                visible = true;
                                goto check_areas_done;
                            }

                            if (matrix[pitchLevel + 1][(yawLevel + 1) % 31][x + dx + 25 + 1][z + dz + 25 + 1]) {
                                visible = true;
                                goto check_areas_done;
                            }
                        }
                    }

                check_areas_done:
                    _World3D.visibilityMatrix[pitchLevel][yawLevel][x + 25][z + 25] = visible;
                }
            }
        }
    }
}

bool world3d_test_point(int x, int z, int y) {
    int px = (z * _World3D.sinEyeYaw + x * _World3D.cosEyeYaw) >> 16;
    int tmp = (z * _World3D.cosEyeYaw - x * _World3D.sinEyeYaw) >> 16;
    int pz = (y * _World3D.sinEyePitch + tmp * _World3D.cosEyePitch) >> 16;
    int py = (y * _World3D.cosEyePitch - tmp * _World3D.sinEyePitch) >> 16;
    if (pz < 50 || pz > 3500) {
        return false;
    }

    int viewportX = _World3D.viewportCenterX + (px << 9) / pz;
    int viewportY = _World3D.viewportCenterY + (py << 9) / pz;
    return viewportX >= _World3D.viewportLeft && viewportX <= _World3D.viewportRight && viewportY >= _World3D.viewportTop && viewportY <= _World3D.viewportBottom;
}

void world3d_reset(World3D *world3d) {
    for (int level = 0; level < world3d->maxLevel; level++) {
        for (int x = 0; x < world3d->maxTileX; x++) {
            for (int z = 0; z < world3d->maxTileZ; z++) {
                if (world3d->levelTiles[level][x][z]) {
                    ground_free(world3d->levelTiles[level][x][z]);
                    world3d->levelTiles[level][x][z] = NULL;
                }
            }
        }
    }

    for (int l = 0; l < LEVEL_COUNT; l++) {
        for (int o = 0; o < _World3D.levelOccluderCount[l]; o++) {
            free(_World3D.levelOccluders[l][o]);
            _World3D.levelOccluders[l][o] = NULL;
        }

        _World3D.levelOccluderCount[l] = 0;
    }

    for (int i = 0; i < world3d->temporaryLocCount; i++) {
        world3d->temporaryLocs[i] = NULL;
    }

    world3d->temporaryLocCount = 0;

    for (int i = 0; i < LOCBUFFER_COUNT; i++) {
        _World3D.locBuffer[i] = NULL;
    }
}

void world3d_set_minlevel(World3D *world3d, int level) {
    world3d->minLevel = level;

    for (int stx = 0; stx < world3d->maxTileX; stx++) {
        for (int stz = 0; stz < world3d->maxTileZ; stz++) {
            if (world3d->levelTiles[level][stx][stz]) {
                ground_free(world3d->levelTiles[level][stx][stz]);
            }
            world3d->levelTiles[level][stx][stz] = ground_new(level, stx, stz);
        }
    }
}

void world3d_set_bridge(World3D *world3d, int stx, int stz) {
    Ground *ground = world3d->levelTiles[0][stx][stz];
    for (int level = 0; level < 3; level++) {
        world3d->levelTiles[level][stx][stz] = world3d->levelTiles[level + 1][stx][stz];
        if (world3d->levelTiles[level][stx][stz]) {
            world3d->levelTiles[level][stx][stz]->level--;
        }
    }

    if (!world3d->levelTiles[0][stx][stz]) {
        world3d->levelTiles[0][stx][stz] = ground_new(0, stx, stz);
    }

    world3d->levelTiles[0][stx][stz]->bridge = ground;
    if (world3d->levelTiles[3][stx][stz]) {
        ground_free(world3d->levelTiles[3][stx][stz]);
        world3d->levelTiles[3][stx][stz] = NULL;
    }
}

void world3d_set_drawlevel(World3D *world3d, int level, int stx, int stz, int drawLevel) {
    Ground *tile = world3d->levelTiles[level][stx][stz];
    if (!tile) {
        return;
    }

    world3d->levelTiles[level][stx][stz]->drawLevel = drawLevel;
}

void world3d_set_tile(World3D *world3d, int level, int x, int z, int shape, int angle, int textureId, int southwestY, int southeastY, int northeastY, int northwestY, int southwestColor, int southeastColor, int northeastColor, int northwestColor, int southwestColor2, int southeastColor2, int northeastColor2, int northwestColor2, int backgroundRgb, int foregroundRgb) {
    TileUnderlay *underlay;
    int l;
    if (shape == 0) {
        underlay = tileunderlay_new(southwestColor, southeastColor, northeastColor, northwestColor, -1, backgroundRgb, false);
        for (l = level; l >= 0; l--) {
            if (!world3d->levelTiles[l][x][z]) {
                world3d->levelTiles[l][x][z] = ground_new(l, x, z);
            }
        }
        world3d->levelTiles[level][x][z]->underlay = underlay;
    } else if (shape == 1) {
        underlay = tileunderlay_new(southwestColor2, southeastColor2, northeastColor2, northwestColor2, textureId, foregroundRgb, southwestY == southeastY && southwestY == northeastY && southwestY == northwestY);
        for (l = level; l >= 0; l--) {
            if (!world3d->levelTiles[l][x][z]) {
                world3d->levelTiles[l][x][z] = ground_new(l, x, z);
            }
        }
        world3d->levelTiles[level][x][z]->underlay = underlay;
    } else {
        TileOverlay *overlay = tileoverlay_new(x, shape, southeastColor2, southeastY, northeastColor, angle, southwestColor, northwestY, foregroundRgb, southwestColor2, textureId, northwestColor2, backgroundRgb, northeastY, northeastColor2, northwestColor, southwestY, z, southeastColor);
        for (l = level; l >= 0; l--) {
            if (!world3d->levelTiles[l][x][z]) {
                world3d->levelTiles[l][x][z] = ground_new(l, x, z);
            }
        }
        world3d->levelTiles[level][x][z]->overlay = overlay;
    }
}

void world3d_add_grounddecoration(World3D *world3d, Model *model, int tileLevel, int tileX, int tileZ, int y, int bitset, int8_t info) {
    GroundDecor *decor = calloc(1, sizeof(GroundDecor));
    decor->model = model;
    decor->x = tileX * 128 + 64;
    decor->z = tileZ * 128 + 64;
    decor->y = y;
    decor->bitset = bitset;
    decor->info = info;
    if (!world3d->levelTiles[tileLevel][tileX][tileZ]) {
        world3d->levelTiles[tileLevel][tileX][tileZ] = ground_new(tileLevel, tileX, tileZ);
    }
    world3d->levelTiles[tileLevel][tileX][tileZ]->groundDecor = decor;
}

void world3d_add_objstack(World3D *world3d, int stx, int stz, int y, int level, int bitset, Model *topObj, Model *middleObj, Model *bottomObj) {
    GroundObject *stack = calloc(1, sizeof(GroundObject));
    stack->topObj = topObj;
    stack->x = stx * 128 + 64;
    stack->z = stz * 128 + 64;
    stack->y = y;
    stack->bitset = bitset;
    stack->bottomObj = bottomObj;
    stack->middleObj = middleObj;
    int stackOffset = 0;
    Ground *tile = world3d->levelTiles[level][stx][stz];
    if (tile) {
        for (int l = 0; l < tile->locCount; l++) {
            int height = tile->locs[l]->model->obj_raise;
            if (height > stackOffset) {
                stackOffset = height;
            }
        }
    }
    stack->offset = stackOffset;
    if (!world3d->levelTiles[level][stx][stz]) {
        world3d->levelTiles[level][stx][stz] = ground_new(level, stx, stz);
    }
    world3d->levelTiles[level][stx][stz]->groundObj = stack;
}

void world3d_add_wall(World3D *world3d, int level, int tileX, int tileZ, int y, int typeA, int typeB, Model *modelA, Model *modelB, int bitset, int8_t info) {
    if (!modelA && !modelB) {
        return;
    }

    Wall *wall = calloc(1, sizeof(Wall));
    wall->bitset = bitset;
    wall->info = info;
    wall->x = tileX * 128 + 64;
    wall->z = tileZ * 128 + 64;
    wall->y = y;
    wall->modelA = modelA;
    wall->modelB = modelB;
    wall->typeA = typeA;
    wall->typeB = typeB;
    for (int l = level; l >= 0; l--) {
        if (!world3d->levelTiles[l][tileX][tileZ]) {
            world3d->levelTiles[l][tileX][tileZ] = ground_new(l, tileX, tileZ);
        }
    }
    world3d->levelTiles[level][tileX][tileZ]->wall = wall;
}

void world3d_set_walldecoration(World3D *world3d, int level, int tileX, int tileZ, int y, int offsetX, int offsetZ, int bitset, Model *model, int8_t info, int angle, int type) {
    if (!model) {
        return;
    }

    Decor *decor = calloc(1, sizeof(Decor));
    decor->bitset = bitset;
    decor->info = info;
    decor->x = tileX * 128 + offsetX + 64;
    decor->z = tileZ * 128 + offsetZ + 64;
    decor->y = y;
    decor->model = model;
    decor->type = type;
    decor->angle = angle;
    for (int l = level; l >= 0; l--) {
        if (!world3d->levelTiles[l][tileX][tileZ]) {
            world3d->levelTiles[l][tileX][tileZ] = ground_new(l, tileX, tileZ);
        }
    }
    world3d->levelTiles[level][tileX][tileZ]->decor = decor;
}

bool world3d_add_loc(World3D *world3d, int level, int tileX, int tileZ, int y, Model *model, Entity *entity, int bitset, int8_t info, int width, int length, int yaw) {
    if (!model && !entity) {
        return true;
    } else {
        int sceneX = tileX * 128 + width * 64;
        int sceneZ = tileZ * 128 + length * 64;
        return world3d_add_loc2(world3d, sceneX, sceneZ, y, level, tileX, tileZ, width, length, model, entity, bitset, info, yaw, false);
    }
}

bool world3d_add_temporary(World3D *world3d, int level, int x, int y, int z, Model *model, Entity *entity, int bitset, int yaw, int padding, bool forwardPadding) {
    if (!model && !entity) {
        return true;
    }
    int x0 = x - padding;
    int z0 = z - padding;
    int x1 = x + padding;
    int z1 = z + padding;
    if (forwardPadding) {
        if (yaw > 640 && yaw < 1408) {
            z1 += 128;
        }
        if (yaw > 1152 && yaw < 1920) {
            x1 += 128;
        }
        if (yaw > 1664 || yaw < 384) {
            z0 -= 128;
        }
        if (yaw > 128 && yaw < 896) {
            x0 -= 128;
        }
    }
    x0 /= 128;
    z0 /= 128;
    x1 /= 128;
    z1 /= 128;
    return world3d_add_loc2(world3d, x, z, y, level, x0, z0, x1 + 1 - x0, z1 - z0 + 1, model, entity, bitset, (int8_t)0, yaw, true);
}

bool world3d_add_temporary2(World3D *world3d, int level, int x, int y, int z, int minTileX, int minTileZ, int maxTileX, int maxTileZ, Model *model, Entity *entity, int bitset, int yaw) {
    return (!model && !entity) || world3d_add_loc2(world3d, x, z, y, level, minTileX, minTileZ, maxTileX + 1 - minTileX, maxTileZ - minTileZ + 1, model, entity, bitset, (int8_t)0, yaw, true);
}

bool world3d_add_loc2(World3D *world3d, int x, int z, int y, int level, int tileX, int tileZ, int tileSizeX, int tileSizeZ, Model *model, Entity *entity, int bitset, int8_t info, int yaw, bool temporary) {
    if (!model && !entity) {
        return false;
    }
    for (int tx = tileX; tx < tileX + tileSizeX; tx++) {
        for (int tz = tileZ; tz < tileZ + tileSizeZ; tz++) {
            if (tx < 0 || tz < 0 || tx >= world3d->maxTileX || tz >= world3d->maxTileZ) {
                return false;
            }
            Ground *tile = world3d->levelTiles[level][tx][tz];
            if (tile && tile->locCount >= 5) {
                return false;
            }
        }
    }
    Location *loc = calloc(1, sizeof(Location));
    loc->bitset = bitset;
    loc->info = info;
    loc->level = level;
    loc->x = x;
    loc->z = z;
    loc->y = y;
    loc->model = model;
    loc->entity = entity;
    loc->yaw = yaw;
    loc->minSceneTileX = tileX;
    loc->minSceneTileZ = tileZ;
    loc->maxSceneTileX = tileX + tileSizeX - 1;
    loc->maxSceneTileZ = tileZ + tileSizeZ - 1;
    for (int tx = tileX; tx < tileX + tileSizeX; tx++) {
        for (int tz = tileZ; tz < tileZ + tileSizeZ; tz++) {
            int spans = 0;
            if (tx > tileX) {
                spans |= 0x1;
            }
            if (tx < tileX + tileSizeX - 1) {
                spans += 0x4;
            }
            if (tz > tileZ) {
                spans += 0x8;
            }
            if (tz < tileZ + tileSizeZ - 1) {
                spans += 0x2;
            }
            for (int l = level; l >= 0; l--) {
                if (!world3d->levelTiles[l][tx][tz]) {
                    world3d->levelTiles[l][tx][tz] = ground_new(l, tx, tz);
                }
            }
            Ground *tile = world3d->levelTiles[level][tx][tz];
            tile->locs[tile->locCount] = loc;
            tile->locSpan[tile->locCount] = spans;
            tile->locSpans |= spans;
            tile->locCount++;
        }
    }

    if (temporary) {
        world3d->temporaryLocs[world3d->temporaryLocCount++] = loc;
    }

    return true;
}

void world3d_clear_temporarylocs(World3D *world3d) {
    for (int i = 0; i < world3d->temporaryLocCount; i++) {
        Location *loc = world3d->temporaryLocs[i];
        world3d_remove_loc2(world3d, loc);
        world3d->temporaryLocs[i] = NULL;
    }

    world3d->temporaryLocCount = 0;
}

void world3d_remove_loc2(World3D *world3d, Location *loc) {
    for (int tx = loc->minSceneTileX; tx <= loc->maxSceneTileX; tx++) {
        for (int tz = loc->minSceneTileZ; tz <= loc->maxSceneTileZ; tz++) {
            Ground *tile = world3d->levelTiles[loc->level][tx][tz];
            if (!tile) {
                continue;
            }

            for (int i = 0; i < tile->locCount; i++) {
                if (tile->locs[i] == loc) {
                    tile->locCount--;
                    for (int j = i; j < tile->locCount; j++) {
                        tile->locs[j] = tile->locs[j + 1];
                        tile->locSpan[j] = tile->locSpan[j + 1];
                    }
                    tile->locs[tile->locCount] = NULL;
                    break;
                }
            }

            tile->locSpans = 0;

            for (int i = 0; i < tile->locCount; i++) {
                tile->locSpans |= tile->locSpan[i];
            }
        }
    }
    free(loc);
}

void world3d_set_locmodel(World3D *world3d, int level, int x, int z, Model *model) {
    if (!model) {
        return;
    }

    Ground *tile = world3d->levelTiles[level][x][z];
    if (!tile) {
        return;
    }

    for (int i = 0; i < tile->locCount; i++) {
        Location *loc = tile->locs[i];
        if ((loc->bitset >> 29 & 0x3) == 2) {
            loc->model = model;
            return;
        }
    }
}

void world3d_set_walldecorationoffset(World3D *world3d, int level, int x, int z, int offset) {
    Ground *tile = world3d->levelTiles[level][x][z];
    if (!tile) {
        return;
    }

    Decor *decor = tile->decor;
    if (!decor) {
        return;
    }

    int sx = x * 128 + 64;
    int sz = z * 128 + 64;
    decor->x = sx + (decor->x - sx) * offset / 16;
    decor->z = sz + (decor->z - sz) * offset / 16;
}

void world3d_set_walldecorationmodel(World3D *world3d, int level, int x, int z, Model *model) {
    if (!model) {
        return;
    }

    Ground *tile = world3d->levelTiles[level][x][z];
    if (!tile) {
        return;
    }

    Decor *decor = tile->decor;
    if (!decor) {
        return;
    }

    decor->model = model;
}

void world3d_set_grounddecorationmodel(World3D *world3d, int level, int x, int z, Model *model) {
    if (!model) {
        return;
    }

    Ground *tile = world3d->levelTiles[level][x][z];
    if (!tile) {
        return;
    }

    GroundDecor *decor = tile->groundDecor;
    if (!decor) {
        return;
    }

    decor->model = model;
}

void world3d_set_wallmodel(World3D *world3d, int level, int x, int z, Model *model) {
    if (!model) {
        return;
    }

    Ground *tile = world3d->levelTiles[level][x][z];
    if (!tile) {
        return;
    }

    Wall *wall = tile->wall;
    if (!wall) {
        return;
    }

    wall->modelA = model;
}

void world3d_set_wallmodels(World3D *world3d, int x, int z, int level, Model *modelA, Model *modelB) {
    if (!modelA) {
        return;
    }

    Ground *tile = world3d->levelTiles[level][x][z];
    if (!tile) {
        return;
    }

    Wall *wall = tile->wall;
    if (!wall) {
        return;
    }

    wall->modelA = modelA;
    wall->modelB = modelB;
}

void world3d_remove_wall(World3D *world3d, int level, int x, int z, int force) {
    Ground *tile = world3d->levelTiles[level][x][z];
    if (force == 1 && tile) {
        free(tile->wall);
        tile->wall = NULL;
    }
}

void world3d_remove_walldecoration(World3D *world3d, int level, int x, int z) {
    Ground *tile = world3d->levelTiles[level][x][z];
    if (!tile) {
        return;
    }

    free(tile->decor);
    tile->decor = NULL;
}

void world3d_remove_loc(World3D *world3d, int level, int x, int z) {
    Ground *tile = world3d->levelTiles[level][x][z];
    if (!tile) {
        return;
    }

    for (int l = 0; l < tile->locCount; l++) {
        Location *loc = tile->locs[l];
        if ((loc->bitset >> 29 & 0x3) == 2 && loc->minSceneTileX == x && loc->minSceneTileZ == z) {
            world3d_remove_loc2(world3d, loc);
            return;
        }
    }
}

void world3d_remove_grounddecoration(World3D *world3d, int level, int x, int z) {
    Ground *tile = world3d->levelTiles[level][x][z];
    if (!tile) {
        return;
    }

    free(tile->groundDecor);
    tile->groundDecor = NULL;
}

void world3d_remove_objstack(World3D *world3d, int level, int x, int z) {
    Ground *tile = world3d->levelTiles[level][x][z];
    if (!tile) {
        return;
    }

    free(tile->groundObj);
    tile->groundObj = NULL;
}

int world3d_get_wallbitset(World3D *world3d, int level, int x, int z) {
    Ground *tile = world3d->levelTiles[level][x][z];
    return !tile || !tile->wall ? 0 : tile->wall->bitset;
}

int world3d_get_walldecorationbitset(World3D *world3d, int level, int z, int x) {
    Ground *tile = world3d->levelTiles[level][x][z];
    return !tile || !tile->decor ? 0 : tile->decor->bitset;
}

int world3d_get_locbitset(World3D *world3d, int level, int x, int z) {
    Ground *tile = world3d->levelTiles[level][x][z];
    if (!tile) {
        return 0;
    }

    for (int l = 0; l < tile->locCount; l++) {
        Location *loc = tile->locs[l];
        if ((loc->bitset >> 29 & 0x3) == 2 && loc->minSceneTileX == x && loc->minSceneTileZ == z) {
            return loc->bitset;
        }
    }

    return 0;
}

int world3d_get_grounddecorationbitset(World3D *world3d, int level, int x, int z) {
    Ground *tile = world3d->levelTiles[level][x][z];
    return !tile || !tile->groundDecor ? 0 : tile->groundDecor->bitset;
}

int world3d_get_info(World3D *world3d, int level, int x, int z, int bitset) {
    Ground *tile = world3d->levelTiles[level][x][z];
    if (!tile) {
        return -1;
    } else if (tile->wall && tile->wall->bitset == bitset) {
        return tile->wall->info & 0xff;
    } else if (tile->decor && tile->decor->bitset == bitset) {
        return tile->decor->info & 0xff;
    } else if (tile->groundDecor && tile->groundDecor->bitset == bitset) {
        return tile->groundDecor->info & 0xff;
    } else {
        for (int i = 0; i < tile->locCount; i++) {
            if (tile->locs[i]->bitset == bitset) {
                return tile->locs[i]->info & 0xff;
            }
        }

        return -1;
    }
}

void world3d_build_models(World3D *world3d, int lightAmbient, int lightAttenuation, int lightSrcX, int lightSrcY, int lightSrcZ) {
    int lightMagnitude = (int)sqrt(lightSrcX * lightSrcX + lightSrcY * lightSrcY + lightSrcZ * lightSrcZ);
    int attenuation = lightAttenuation * lightMagnitude >> 8;

    for (int level = 0; level < world3d->maxLevel; level++) {
        for (int tileX = 0; tileX < world3d->maxTileX; tileX++) {
            for (int tileZ = 0; tileZ < world3d->maxTileZ; tileZ++) {
                Ground *tile = world3d->levelTiles[level][tileX][tileZ];
                if (!tile) {
                    continue;
                }

                Wall *wall = tile->wall;
                if (wall && wall->modelA && wall->modelA->vertex_normal) {
                    world3d_merge_locnormals(world3d, level, tileX, tileZ, 1, 1, wall->modelA);
                    if (wall->modelB && wall->modelB->vertex_normal) {
                        world3d_merge_locnormals(world3d, level, tileX, tileZ, 1, 1, wall->modelB);
                        world3d_merge_normals(world3d, wall->modelA, wall->modelB, 0, 0, 0, false);
                        model_apply_lighting(wall->modelB, lightAmbient, attenuation, lightSrcX, lightSrcY, lightSrcZ);
                    }
                    model_apply_lighting(wall->modelA, lightAmbient, attenuation, lightSrcX, lightSrcY, lightSrcZ);
                }

                for (int i = 0; i < tile->locCount; i++) {
                    Location *loc = tile->locs[i];
                    if (loc && loc->model && loc->model->vertex_normal) {
                        world3d_merge_locnormals(world3d, level, tileX, tileZ, loc->maxSceneTileX + 1 - loc->minSceneTileX, loc->maxSceneTileZ - loc->minSceneTileZ + 1, loc->model);
                        model_apply_lighting(loc->model, lightAmbient, attenuation, lightSrcX, lightSrcY, lightSrcZ);
                    }
                }

                GroundDecor *decor = tile->groundDecor;
                if (decor && decor->model->vertex_normal) {
                    world3d_merge_grounddecorationnormals(world3d, level, tileX, tileZ, decor->model);
                    model_apply_lighting(decor->model, lightAmbient, attenuation, lightSrcX, lightSrcY, lightSrcZ);
                }
            }
        }
    }
}

void world3d_merge_grounddecorationnormals(World3D *world3d, int level, int tileX, int tileZ, Model *model) {
    Ground *tile;
    if (tileX < world3d->maxTileX) {
        tile = world3d->levelTiles[level][tileX + 1][tileZ];
        if (tile && tile->groundDecor && tile->groundDecor->model->vertex_normal) {
            world3d_merge_normals(world3d, model, tile->groundDecor->model, 128, 0, 0, true);
        }
    }

    if (tileZ < world3d->maxTileX) {
        tile = world3d->levelTiles[level][tileX][tileZ + 1];
        if (tile && tile->groundDecor && tile->groundDecor->model->vertex_normal) {
            world3d_merge_normals(world3d, model, tile->groundDecor->model, 0, 0, 128, true);
        }
    }

    if (tileX < world3d->maxTileX && tileZ < world3d->maxTileZ) {
        tile = world3d->levelTiles[level][tileX + 1][tileZ + 1];
        if (tile && tile->groundDecor && tile->groundDecor->model->vertex_normal) {
            world3d_merge_normals(world3d, model, tile->groundDecor->model, 128, 0, 128, true);
        }
    }

    if (tileX < world3d->maxTileX && tileZ > 0) {
        tile = world3d->levelTiles[level][tileX + 1][tileZ - 1];
        if (tile && tile->groundDecor && tile->groundDecor->model->vertex_normal) {
            world3d_merge_normals(world3d, model, tile->groundDecor->model, 128, 0, -128, true);
        }
    }
}

void world3d_merge_locnormals(World3D *world3d, int level, int tileX, int tileZ, int tileSizeX, int tileSizeZ, Model *model) {
    bool allowFaceRemoval = true;

    int minTileX = tileX;
    int maxTileX = tileX + tileSizeX;
    int minTileZ = tileZ - 1;
    int maxTileZ = tileZ + tileSizeZ;

    for (int l = level; l <= level + 1; l++) {
        if (l == world3d->maxLevel) {
            continue;
        }

        for (int x = minTileX; x <= maxTileX; x++) {
            if (x < 0 || x >= world3d->maxTileX) {
                continue;
            }

            for (int z = minTileZ; z <= maxTileZ; z++) {
                if (z < 0 || z >= world3d->maxTileZ || (allowFaceRemoval && x < maxTileX && z < maxTileZ && (z >= tileZ || x == tileX))) {
                    continue;
                }

                Ground *tile = world3d->levelTiles[l][x][z];
                if (!tile) {
                    continue;
                }

                int offsetY = (world3d->levelHeightmaps[l][x][z] + world3d->levelHeightmaps[l][x + 1][z] + world3d->levelHeightmaps[l][x][z + 1] + world3d->levelHeightmaps[l][x + 1][z + 1]) / 4 - (world3d->levelHeightmaps[level][tileX][tileZ] + world3d->levelHeightmaps[level][tileX + 1][tileZ] + world3d->levelHeightmaps[level][tileX][tileZ + 1] + world3d->levelHeightmaps[level][tileX + 1][tileZ + 1]) / 4;

                Wall *wall = tile->wall;
                if (wall && wall->modelA && wall->modelA->vertex_normal) {
                    world3d_merge_normals(world3d, model, wall->modelA, (x - tileX) * 128 + (1 - tileSizeX) * 64, offsetY, (z - tileZ) * 128 + (1 - tileSizeZ) * 64, allowFaceRemoval);
                }

                if (wall && wall->modelB && wall->modelB->vertex_normal) {
                    world3d_merge_normals(world3d, model, wall->modelB, (x - tileX) * 128 + (1 - tileSizeX) * 64, offsetY, (z - tileZ) * 128 + (1 - tileSizeZ) * 64, allowFaceRemoval);
                }

                for (int i = 0; i < tile->locCount; i++) {
                    Location *loc = tile->locs[i];
                    if (!loc || !loc->model || !loc->model->vertex_normal) {
                        continue;
                    }

                    int locTileSizeX = loc->maxSceneTileX + 1 - loc->minSceneTileX;
                    int locTileSizeZ = loc->maxSceneTileZ + 1 - loc->minSceneTileZ;
                    world3d_merge_normals(world3d, model, loc->model, (loc->minSceneTileX - tileX) * 128 + (locTileSizeX - tileSizeX) * 64, offsetY, (loc->minSceneTileZ - tileZ) * 128 + (locTileSizeZ - tileSizeZ) * 64, allowFaceRemoval);
                }
            }
        }

        minTileX--;
        allowFaceRemoval = false;
    }
}

void world3d_merge_normals(World3D *world3d, Model *modelA, Model *modelB, int offsetX, int offsetY, int offsetZ, bool allowFaceRemoval) {
    world3d->tmpMergeIndex++;

    int merged = 0;
    int *vertexX = modelB->vertices_x;
    int vertexCountB = modelB->vertex_count;

    for (int vertexA = 0; vertexA < modelA->vertex_count; vertexA++) {
        VertexNormal *normalA = modelA->vertex_normal[vertexA];
        VertexNormal *originalNormalA = modelA->vertex_normal_original[vertexA];
        if (originalNormalA->w != 0) {
            int y = modelA->vertices_y[vertexA] - offsetY;
            if (y > modelB->min_y) {
                continue;
            }

            int x = modelA->vertices_x[vertexA] - offsetX;
            if (x < modelB->min_x || x > modelB->max_x) {
                continue;
            }

            int z = modelA->vertices_z[vertexA] - offsetZ;
            if (z < modelB->min_z || z > modelB->max_z) {
                continue;
            }

            for (int vertexB = 0; vertexB < vertexCountB; vertexB++) {
                VertexNormal *normalB = modelB->vertex_normal[vertexB];
                VertexNormal *originalNormalB = modelB->vertex_normal_original[vertexB];
                if (x != vertexX[vertexB] || z != modelB->vertices_z[vertexB] || y != modelB->vertices_y[vertexB] || originalNormalB->w == 0) {
                    continue;
                }

                normalA->x += originalNormalB->x;
                normalA->y += originalNormalB->y;
                normalA->z += originalNormalB->z;
                normalA->w += originalNormalB->w;
                normalB->x += originalNormalA->x;
                normalB->y += originalNormalA->y;
                normalB->z += originalNormalA->z;
                normalB->w += originalNormalA->w;
                merged++;
                world3d->mergeIndexA[vertexA] = world3d->tmpMergeIndex;
                world3d->mergeIndexB[vertexB] = world3d->tmpMergeIndex;
            }
        }
    }

    if (merged < 3 || !allowFaceRemoval) {
        return;
    }

    for (int i = 0; i < modelA->face_count; i++) {
        if (world3d->mergeIndexA[modelA->face_indices_a[i]] == world3d->tmpMergeIndex && world3d->mergeIndexA[modelA->face_indices_b[i]] == world3d->tmpMergeIndex && world3d->mergeIndexA[modelA->face_indices_c[i]] == world3d->tmpMergeIndex) {
            modelA->face_infos[i] = -1;
        }
    }

    for (int i = 0; i < modelB->face_count; i++) {
        if (world3d->mergeIndexB[modelB->face_indices_a[i]] == world3d->tmpMergeIndex && world3d->mergeIndexB[modelB->face_indices_b[i]] == world3d->tmpMergeIndex && world3d->mergeIndexB[modelB->face_indices_c[i]] == world3d->tmpMergeIndex) {
            modelB->face_infos[i] = -1;
        }
    }
}

void world3d_draw_minimaptile(World3D *world3d, int level, int x, int z, int *dst, int offset, int step) {
    Ground *tile = world3d->levelTiles[level][x][z];
    if (!tile) {
        return;
    }

    TileUnderlay *underlay = tile->underlay;
    if (underlay) {
        int rgb = underlay->rgb;
        if (rgb == 0) {
            return;
        }

        for (int i = 0; i < 4; i++) {
            dst[offset] = rgb;
            dst[offset + 1] = rgb;
            dst[offset + 2] = rgb;
            dst[offset + 3] = rgb;
            offset += step;
        }

        return;
    }

    TileOverlay *overlay = tile->overlay;
    if (!overlay) {
        return;
    }

    int shape = overlay->shape;
    int angle = overlay->rotation;
    int background = overlay->backgroundRgb;
    int foreground = overlay->foregroundRgb;
    const int *mask = MINIMAP_OVERLAY_SHAPE[shape];
    const int *rotation = MINIMAP_OVERLAY_ROTATION[angle];

    int off = 0;
    if (background != 0) {
        for (int i = 0; i < 4; i++) {
            dst[offset] = mask[rotation[off++]] == 0 ? background : foreground;
            dst[offset + 1] = mask[rotation[off++]] == 0 ? background : foreground;
            dst[offset + 2] = mask[rotation[off++]] == 0 ? background : foreground;
            dst[offset + 3] = mask[rotation[off++]] == 0 ? background : foreground;
            offset += step;
        }

        return;
    }

    for (int i = 0; i < 4; i++) {
        if (mask[rotation[off++]] != 0) {
            dst[offset] = foreground;
        }

        if (mask[rotation[off++]] != 0) {
            dst[offset + 1] = foreground;
        }

        if (mask[rotation[off++]] != 0) {
            dst[offset + 2] = foreground;
        }

        if (mask[rotation[off++]] != 0) {
            dst[offset + 3] = foreground;
        }

        offset += step;
    }
}

void world3d_click(int mouseX, int mouseY) {
    _World3D.takingInput = true;
    _World3D.mouseX = mouseX;
    _World3D.mouseY = mouseY;
    _World3D.clickTileX = -1;
    _World3D.clickTileZ = -1;
}

void world3d_draw(World3D *world3d, int eyeX, int eyeY, int eyeZ, int topLevel, int eyeYaw, int eyePitch, int loopCycle) {
    if (eyeX < 0) {
        eyeX = 0;
    } else if (eyeX >= world3d->maxTileX * 128) {
        eyeX = world3d->maxTileX * 128 - 1;
    }

    if (eyeZ < 0) {
        eyeZ = 0;
    } else if (eyeZ >= world3d->maxTileZ * 128) {
        eyeZ = world3d->maxTileZ * 128 - 1;
    }

    _World3D.cycle++;
    _World3D.sinEyePitch = _Pix3D.sin_table[eyePitch];
    _World3D.cosEyePitch = _Pix3D.cos_table[eyePitch];
    _World3D.sinEyeYaw = _Pix3D.sin_table[eyeYaw];
    _World3D.cosEyeYaw = _Pix3D.cos_table[eyeYaw];

    _World3D.visibilityMap = _World3D.visibilityMatrix[(eyePitch - 128) / 32][eyeYaw / 64];
    _World3D.eyeX = eyeX;
    _World3D.eyeY = eyeY;
    _World3D.eyeZ = eyeZ;
    _World3D.eyeTileX = eyeX / 128;
    _World3D.eyeTileZ = eyeZ / 128;
    _World3D.topLevel = topLevel;

    _World3D.minDrawTileX = _World3D.eyeTileX - 25;
    if (_World3D.minDrawTileX < 0) {
        _World3D.minDrawTileX = 0;
    }

    _World3D.minDrawTileZ = _World3D.eyeTileZ - 25;
    if (_World3D.minDrawTileZ < 0) {
        _World3D.minDrawTileZ = 0;
    }

    _World3D.maxDrawTileX = _World3D.eyeTileX + 25;
    if (_World3D.maxDrawTileX > world3d->maxTileX) {
        _World3D.maxDrawTileX = world3d->maxTileX;
    }

    _World3D.maxDrawTileZ = _World3D.eyeTileZ + 25;
    if (_World3D.maxDrawTileZ > world3d->maxTileZ) {
        _World3D.maxDrawTileZ = world3d->maxTileZ;
    }

    world3d_update_activeoccluders();
    _World3D.tilesRemaining = 0;

    for (int level = world3d->minLevel; level < world3d->maxLevel; level++) {
        Ground ***tiles = world3d->levelTiles[level];
        for (int x = _World3D.minDrawTileX; x < _World3D.maxDrawTileX; x++) {
            for (int z = _World3D.minDrawTileZ; z < _World3D.maxDrawTileZ; z++) {
                Ground *tile = tiles[x][z];
                if (!tile) {
                    continue;
                }

                if (tile->drawLevel <= topLevel && (_World3D.visibilityMap[x + 25 - _World3D.eyeTileX][z + 25 - _World3D.eyeTileZ] || world3d->levelHeightmaps[level][x][z] - eyeY >= 2000)) {
                    tile->visible = true;
                    tile->update = true;
                    tile->containsLocs = tile->locCount > 0;
                    _World3D.tilesRemaining++;
                } else {
                    tile->visible = false;
                    tile->update = false;
                    tile->checkLocSpans = 0;
                }
            }
        }
    }

    for (int level = world3d->minLevel; level < world3d->maxLevel; level++) {
        Ground ***tiles = world3d->levelTiles[level];
        for (int dx = -25; dx <= 0; dx++) {
            int rightTileX = _World3D.eyeTileX + dx;
            int leftTileX = _World3D.eyeTileX - dx;

            if (rightTileX < _World3D.minDrawTileX && leftTileX >= _World3D.maxDrawTileX) {
                continue;
            }

            for (int dz = -25; dz <= 0; dz++) {
                int forwardTileZ = _World3D.eyeTileZ + dz;
                int backwardTileZ = _World3D.eyeTileZ - dz;
                Ground *tile;
                if (rightTileX >= _World3D.minDrawTileX) {
                    if (forwardTileZ >= _World3D.minDrawTileZ) {
                        tile = tiles[rightTileX][forwardTileZ];
                        if (tile && tile->visible) {
                            world3d_draw_tile(world3d, tile, true, loopCycle);
                        }
                    }

                    if (backwardTileZ < _World3D.maxDrawTileZ) {
                        tile = tiles[rightTileX][backwardTileZ];
                        if (tile && tile->visible) {
                            world3d_draw_tile(world3d, tile, true, loopCycle);
                        }
                    }
                }

                if (leftTileX < _World3D.maxDrawTileX) {
                    if (forwardTileZ >= _World3D.minDrawTileZ) {
                        tile = tiles[leftTileX][forwardTileZ];
                        if (tile && tile->visible) {
                            world3d_draw_tile(world3d, tile, true, loopCycle);
                        }
                    }

                    if (backwardTileZ < _World3D.maxDrawTileZ) {
                        tile = tiles[leftTileX][backwardTileZ];
                        if (tile && tile->visible) {
                            world3d_draw_tile(world3d, tile, true, loopCycle);
                        }
                    }
                }

                if (_World3D.tilesRemaining == 0) {
                    _World3D.takingInput = false;
                    return;
                }
            }
        }
    }

    for (int level = world3d->minLevel; level < world3d->maxLevel; level++) {
        Ground ***tiles = world3d->levelTiles[level];
        for (int dx = -25; dx <= 0; dx++) {
            int rightTileX = _World3D.eyeTileX + dx;
            int leftTileX = _World3D.eyeTileX - dx;
            if (rightTileX < _World3D.minDrawTileX && leftTileX >= _World3D.maxDrawTileX) {
                continue;
            }

            for (int dz = -25; dz <= 0; dz++) {
                int forwardTileZ = _World3D.eyeTileZ + dz;
                int backgroundTileZ = _World3D.eyeTileZ - dz;
                Ground *tile;
                if (rightTileX >= _World3D.minDrawTileX) {
                    if (forwardTileZ >= _World3D.minDrawTileZ) {
                        tile = tiles[rightTileX][forwardTileZ];
                        if (tile && tile->visible) {
                            world3d_draw_tile(world3d, tile, false, loopCycle);
                        }
                    }

                    if (backgroundTileZ < _World3D.maxDrawTileZ) {
                        tile = tiles[rightTileX][backgroundTileZ];
                        if (tile && tile->visible) {
                            world3d_draw_tile(world3d, tile, false, loopCycle);
                        }
                    }
                }

                if (leftTileX < _World3D.maxDrawTileX) {
                    if (forwardTileZ >= _World3D.minDrawTileZ) {
                        tile = tiles[leftTileX][forwardTileZ];
                        if (tile && tile->visible) {
                            world3d_draw_tile(world3d, tile, false, loopCycle);
                        }
                    }

                    if (backgroundTileZ < _World3D.maxDrawTileZ) {
                        tile = tiles[leftTileX][backgroundTileZ];
                        if (tile && tile->visible) {
                            world3d_draw_tile(world3d, tile, false, loopCycle);
                        }
                    }
                }

                if (_World3D.tilesRemaining == 0) {
                    _World3D.takingInput = false;
                    return;
                }
            }
        }
    }
}

void world3d_draw_tile(World3D *world3d, Ground *next, bool checkAdjacent, int loopCycle) {
    linklist_add_tail(_World3D.drawTileQueue, &next->link);

    while (true) {
        Ground *tile;

        do {
            tile = (Ground *)linklist_remove_head(_World3D.drawTileQueue);

            if (!tile) {
                return;
            }
        } while (!tile->update);

        int tileX = tile->x;
        int tileZ = tile->z;
        int level = tile->level;
        int occludeLevel = tile->occludeLevel;
        Ground ***tiles = world3d->levelTiles[level];

        if (tile->visible) {
            if (checkAdjacent) {
                if (level > 0) {
                    Ground *above = world3d->levelTiles[level - 1][tileX][tileZ];

                    if (above && above->update) {
                        continue;
                    }
                }

                if (tileX <= _World3D.eyeTileX && tileX > _World3D.minDrawTileX) {
                    Ground *adjacent = tiles[tileX - 1][tileZ];

                    if (adjacent && adjacent->update && (adjacent->visible || (tile->locSpans & 0x1) == 0)) {
                        continue;
                    }
                }

                if (tileX >= _World3D.eyeTileX && tileX < _World3D.maxDrawTileX - 1) {
                    Ground *adjacent = tiles[tileX + 1][tileZ];

                    if (adjacent && adjacent->update && (adjacent->visible || (tile->locSpans & 0x4) == 0)) {
                        continue;
                    }
                }

                if (tileZ <= _World3D.eyeTileZ && tileZ > _World3D.minDrawTileZ) {
                    Ground *adjacent = tiles[tileX][tileZ - 1];

                    if (adjacent && adjacent->update && (adjacent->visible || (tile->locSpans & 0x8) == 0)) {
                        continue;
                    }
                }

                if (tileZ >= _World3D.eyeTileZ && tileZ < _World3D.maxDrawTileZ - 1) {
                    Ground *adjacent = tiles[tileX][tileZ + 1];

                    if (adjacent && adjacent->update && (adjacent->visible || (tile->locSpans & 0x2) == 0)) {
                        continue;
                    }
                }
            } else {
                checkAdjacent = true;
            }

            tile->visible = false;

            if (tile->bridge) {
                Ground *bridge = tile->bridge;

                if (!bridge->underlay) {
                    if (bridge->overlay && !world3d_tile_visible(world3d, 0, tileX, tileZ)) {
                        world3d_draw_tileoverlay(tileX, tileZ, bridge->overlay, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw);
                    }
                } else if (!world3d_tile_visible(world3d, 0, tileX, tileZ)) {
                    world3d_draw_tileunderlay(world3d, bridge->underlay, 0, tileX, tileZ, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw);
                }

                Wall *wall = bridge->wall;
                if (wall) {
                    model_draw(wall->modelA, 0, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, wall->x - _World3D.eyeX, wall->y - _World3D.eyeY, wall->z - _World3D.eyeZ, wall->bitset);
                }

                for (int i = 0; i < bridge->locCount; i++) {
                    Location *loc = bridge->locs[i];

                    if (loc) {
                        Model *model = loc->model;
                        bool _free = false;
                        if (!model) {
                            model = entity_draw(loc->entity, loopCycle);
                            _free = true;
                        }

                        model_draw(model, loc->yaw, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, loc->x - _World3D.eyeX, loc->y - _World3D.eyeY, loc->z - _World3D.eyeZ, loc->bitset);
                        if (_free) {
                            entity_draw_free(loc->entity, model, loopCycle);
                        }
                    }
                }
            }

            bool tileDrawn = false;
            if (!tile->underlay) {
                if (tile->overlay && !world3d_tile_visible(world3d, occludeLevel, tileX, tileZ)) {
                    tileDrawn = true;
                    world3d_draw_tileoverlay(tileX, tileZ, tile->overlay, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw);
                }
            } else if (!world3d_tile_visible(world3d, occludeLevel, tileX, tileZ)) {
                tileDrawn = true;
                world3d_draw_tileunderlay(world3d, tile->underlay, occludeLevel, tileX, tileZ, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw);
            }

            int direction = 0;
            int frontWallTypes = 0;

            Wall *wall = tile->wall;
            Decor *decor = tile->decor;

            if (wall || decor) {
                if (_World3D.eyeTileX == tileX) {
                    direction += 1;
                } else if (_World3D.eyeTileX < tileX) {
                    direction += 2;
                }

                if (_World3D.eyeTileZ == tileZ) {
                    direction += 3;
                } else if (_World3D.eyeTileZ > tileZ) {
                    direction += 6;
                }

                frontWallTypes = FRONT_WALL_TYPES[direction];
                tile->backWallTypes = BACK_WALL_TYPES[direction];
            }

            if (wall) {
                if ((wall->typeA & DIRECTION_ALLOW_WALL_CORNER_TYPE[direction]) == 0) {
                    tile->checkLocSpans = 0;
                } else if (wall->typeA == 16) {
                    tile->checkLocSpans = 3;
                    tile->blockLocSpans = WALL_CORNER_TYPE_16_BLOCK_LOC_SPANS[direction];
                    tile->inverseBlockLocSpans = 3 - tile->blockLocSpans;
                } else if (wall->typeA == 32) {
                    tile->checkLocSpans = 6;
                    tile->blockLocSpans = WALL_CORNER_TYPE_32_BLOCK_LOC_SPANS[direction];
                    tile->inverseBlockLocSpans = 6 - tile->blockLocSpans;
                } else if (wall->typeA == 64) {
                    tile->checkLocSpans = 12;
                    tile->blockLocSpans = WALL_CORNER_TYPE_64_BLOCK_LOC_SPANS[direction];
                    tile->inverseBlockLocSpans = 12 - tile->blockLocSpans;
                } else {
                    tile->checkLocSpans = 9;
                    tile->blockLocSpans = WALL_CORNER_TYPE_128_BLOCK_LOC_SPANS[direction];
                    tile->inverseBlockLocSpans = 9 - tile->blockLocSpans;
                }

                if ((wall->typeA & frontWallTypes) != 0 && !world3d_wall_visible(world3d, occludeLevel, tileX, tileZ, wall->typeA)) {
                    model_draw(wall->modelA, 0, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, wall->x - _World3D.eyeX, wall->y - _World3D.eyeY, wall->z - _World3D.eyeZ, wall->bitset);
                }

                if ((wall->typeB & frontWallTypes) != 0 && !world3d_wall_visible(world3d, occludeLevel, tileX, tileZ, wall->typeB)) {
                    model_draw(wall->modelB, 0, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, wall->x - _World3D.eyeX, wall->y - _World3D.eyeY, wall->z - _World3D.eyeZ, wall->bitset);
                }
            }

            if (decor && !world3d_visible(world3d, occludeLevel, tileX, tileZ, decor->model->max_y)) {
                if ((decor->type & frontWallTypes) != 0) {
                    model_draw(decor->model, decor->angle, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, decor->x - _World3D.eyeX, decor->y - _World3D.eyeY, decor->z - _World3D.eyeZ, decor->bitset);
                } else if ((decor->type & 0x300) != 0) {
                    int x = decor->x - _World3D.eyeX;
                    int y = decor->y - _World3D.eyeY;
                    int z = decor->z - _World3D.eyeZ;
                    int rotation = decor->angle;

                    int nearestX;
                    if (rotation == 1 || rotation == 2) {
                        nearestX = -x;
                    } else {
                        nearestX = x;
                    }

                    int nearestZ;
                    if (rotation == 2 || rotation == 3) {
                        nearestZ = -z;
                    } else {
                        nearestZ = z;
                    }

                    if ((decor->type & 0x100) != 0 && nearestZ < nearestX) {
                        int drawX = x + WALL_DECORATION_INSET_X[rotation];
                        int drawZ = z + WALL_DECORATION_INSET_Z[rotation];
                        model_draw(decor->model, rotation * 512 + 256, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, drawX, y, drawZ, decor->bitset);
                    }

                    if ((decor->type & 0x200) != 0 && nearestZ > nearestX) {
                        int drawX = x + WALL_DECORATION_OUTSET_X[rotation];
                        int drawZ = z + WALL_DECORATION_OUTSET_Z[rotation];
                        model_draw(decor->model, rotation * 512 + 1280 & 0x7ff, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, drawX, y, drawZ, decor->bitset);
                    }
                }
            }

            if (tileDrawn) {
                GroundDecor *groundDecor = tile->groundDecor;
                if (groundDecor) {
                    model_draw(groundDecor->model, 0, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, groundDecor->x - _World3D.eyeX, groundDecor->y - _World3D.eyeY, groundDecor->z - _World3D.eyeZ, groundDecor->bitset);
                }

                GroundObject *objs = tile->groundObj;
                if (objs && objs->offset == 0) {
                    if (objs->bottomObj) {
                        model_draw(objs->bottomObj, 0, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, objs->x - _World3D.eyeX, objs->y - _World3D.eyeY, objs->z - _World3D.eyeZ, objs->bitset);
                    }

                    if (objs->middleObj) {
                        model_draw(objs->middleObj, 0, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, objs->x - _World3D.eyeX, objs->y - _World3D.eyeY, objs->z - _World3D.eyeZ, objs->bitset);
                    }

                    if (objs->topObj) {
                        model_draw(objs->topObj, 0, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, objs->x - _World3D.eyeX, objs->y - _World3D.eyeY, objs->z - _World3D.eyeZ, objs->bitset);
                    }
                }
            }

            int spans = tile->locSpans;

            if (spans != 0) {
                if (tileX < _World3D.eyeTileX && (spans & 0x4) != 0) {
                    Ground *adjacent = tiles[tileX + 1][tileZ];
                    if (adjacent && adjacent->update) {
                        linklist_add_tail(_World3D.drawTileQueue, &adjacent->link);
                    }
                }

                if (tileZ < _World3D.eyeTileZ && (spans & 0x2) != 0) {
                    Ground *adjacent = tiles[tileX][tileZ + 1];
                    if (adjacent && adjacent->update) {
                        linklist_add_tail(_World3D.drawTileQueue, &adjacent->link);
                    }
                }

                if (tileX > _World3D.eyeTileX && (spans & 0x1) != 0) {
                    Ground *adjacent = tiles[tileX - 1][tileZ];
                    if (adjacent && adjacent->update) {
                        linklist_add_tail(_World3D.drawTileQueue, &adjacent->link);
                    }
                }

                if (tileZ > _World3D.eyeTileZ && (spans & 0x8) != 0) {
                    Ground *adjacent = tiles[tileX][tileZ - 1];
                    if (adjacent && adjacent->update) {
                        linklist_add_tail(_World3D.drawTileQueue, &adjacent->link);
                    }
                }
            }
        }

        if (tile->checkLocSpans != 0) {
            bool draw = true;
            for (int i = 0; i < tile->locCount; i++) {
                if (tile->locs[i]->cycle != _World3D.cycle && (tile->locSpan[i] & tile->checkLocSpans) == tile->blockLocSpans) {
                    draw = false;
                    break;
                }
            }

            if (draw) {
                Wall *wall = tile->wall;

                if (!world3d_wall_visible(world3d, occludeLevel, tileX, tileZ, wall->typeA)) {
                    model_draw(wall->modelA, 0, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, wall->x - _World3D.eyeX, wall->y - _World3D.eyeY, wall->z - _World3D.eyeZ, wall->bitset);
                }

                tile->checkLocSpans = 0;
            }
        }

        if (tile->containsLocs) {
            int locCount = tile->locCount;
            tile->containsLocs = false;
            int locBufferSize = 0;

            for (int i = 0; i < locCount; i++) {
                Location *loc = tile->locs[i];

                if (loc->cycle == _World3D.cycle) {
                    continue;
                }

                for (int x = loc->minSceneTileX; x <= loc->maxSceneTileX; x++) {
                    for (int z = loc->minSceneTileZ; z <= loc->maxSceneTileZ; z++) {
                        Ground *other = tiles[x][z];

                        if (!other->visible) {
                            if (other->checkLocSpans == 0) {
                                continue;
                            }

                            int spans = 0;

                            if (x > loc->minSceneTileX) {
                                spans += 1;
                            }

                            if (x < loc->maxSceneTileX) {
                                spans += 4;
                            }

                            if (z > loc->minSceneTileZ) {
                                spans += 8;
                            }

                            if (z < loc->maxSceneTileZ) {
                                spans += 2;
                            }

                            if ((spans & other->checkLocSpans) != tile->inverseBlockLocSpans) {
                                continue;
                            }
                        }

                        tile->containsLocs = true;
                        goto iterate_locs;
                    }
                }

                _World3D.locBuffer[locBufferSize++] = loc;

                int minTileDistanceX = _World3D.eyeTileX - loc->minSceneTileX;
                int maxTileDistanceX = loc->maxSceneTileX - _World3D.eyeTileX;

                if (maxTileDistanceX > minTileDistanceX) {
                    minTileDistanceX = maxTileDistanceX;
                }

                int minTileDistanceZ = _World3D.eyeTileZ - loc->minSceneTileZ;
                int maxTileDistanceZ = loc->maxSceneTileZ - _World3D.eyeTileZ;

                if (maxTileDistanceZ > minTileDistanceZ) {
                    loc->distance = minTileDistanceX + maxTileDistanceZ;
                } else {
                    loc->distance = minTileDistanceX + minTileDistanceZ;
                }
            iterate_locs:;
            }

            while (true) {
                int farthestDistance = -50;
                int farthestIndex = -1;

                for (int index = 0; index < locBufferSize; index++) {
                    Location *loc = _World3D.locBuffer[index];

                    if (loc->cycle != _World3D.cycle) {
                        if (loc->distance > farthestDistance) {
                            farthestDistance = loc->distance;
                            farthestIndex = index;
                        }
                    }
                }

                if (farthestIndex == -1) {
                    break;
                }

                Location *farthest = _World3D.locBuffer[farthestIndex];
                farthest->cycle = _World3D.cycle;

                Model *model = farthest->model;
                bool _free = false;
                if (!model) {
                    model = entity_draw(farthest->entity, loopCycle);
                    _free = true;
                }

                if (model) {
                    if (!world3d_loc_visible(world3d, occludeLevel, farthest->minSceneTileX, farthest->maxSceneTileX, farthest->minSceneTileZ, farthest->maxSceneTileZ, model->max_y)) {
                        model_draw(model, farthest->yaw, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, farthest->x - _World3D.eyeX, farthest->y - _World3D.eyeY, farthest->z - _World3D.eyeZ, farthest->bitset);
                        if (_free) {
                            entity_draw_free(farthest->entity, model, loopCycle);
                        }
                    }
                }

                for (int x = farthest->minSceneTileX; x <= farthest->maxSceneTileX; x++) {
                    for (int z = farthest->minSceneTileZ; z <= farthest->maxSceneTileZ; z++) {
                        Ground *occupied = tiles[x][z];

                        if (occupied->checkLocSpans != 0) {
                            linklist_add_tail(_World3D.drawTileQueue, &occupied->link);
                        } else if ((x != tileX || z != tileZ) && occupied->update) {
                            linklist_add_tail(_World3D.drawTileQueue, &occupied->link);
                        }
                    }
                }
            }

            if (tile->containsLocs) {
                continue;
            }
        }

        if (!tile->update || tile->checkLocSpans != 0) {
            continue;
        }

        if (tileX <= _World3D.eyeTileX && tileX > _World3D.minDrawTileX) {
            Ground *adjacent = tiles[tileX - 1][tileZ];
            if (adjacent && adjacent->update) {
                continue;
            }
        }

        if (tileX >= _World3D.eyeTileX && tileX < _World3D.maxDrawTileX - 1) {
            Ground *adjacent = tiles[tileX + 1][tileZ];
            if (adjacent && adjacent->update) {
                continue;
            }
        }

        if (tileZ <= _World3D.eyeTileZ && tileZ > _World3D.minDrawTileZ) {
            Ground *adjacent = tiles[tileX][tileZ - 1];
            if (adjacent && adjacent->update) {
                continue;
            }
        }

        if (tileZ >= _World3D.eyeTileZ && tileZ < _World3D.maxDrawTileZ - 1) {
            Ground *adjacent = tiles[tileX][tileZ + 1];
            if (adjacent && adjacent->update) {
                continue;
            }
        }

        tile->update = false;
        _World3D.tilesRemaining--;

        GroundObject *objs = tile->groundObj;
        if (objs && objs->offset != 0) {
            if (objs->bottomObj) {
                model_draw(objs->bottomObj, 0, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, objs->x - _World3D.eyeX, objs->y - _World3D.eyeY - objs->offset, objs->z - _World3D.eyeZ, objs->bitset);
            }

            if (objs->middleObj) {
                model_draw(objs->middleObj, 0, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, objs->x - _World3D.eyeX, objs->y - _World3D.eyeY - objs->offset, objs->z - _World3D.eyeZ, objs->bitset);
            }

            if (objs->topObj) {
                model_draw(objs->topObj, 0, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, objs->x - _World3D.eyeX, objs->y - _World3D.eyeY - objs->offset, objs->z - _World3D.eyeZ, objs->bitset);
            }
        }

        if (tile->backWallTypes != 0) {
            Decor *decor = tile->decor;

            if (decor && !world3d_visible(world3d, occludeLevel, tileX, tileZ, decor->model->max_y)) {
                if ((decor->type & tile->backWallTypes) != 0) {
                    model_draw(decor->model, decor->angle, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, decor->x - _World3D.eyeX, decor->y - _World3D.eyeY, decor->z - _World3D.eyeZ, decor->bitset);
                } else if ((decor->type & 0x300) != 0) {
                    int x = decor->x - _World3D.eyeX;
                    int y = decor->y - _World3D.eyeY;
                    int z = decor->z - _World3D.eyeZ;
                    int rotation = decor->angle;

                    int nearestX;
                    if (rotation == 1 || rotation == 2) {
                        nearestX = -x;
                    } else {
                        nearestX = x;
                    }

                    int nearestZ;
                    if (rotation == 2 || rotation == 3) {
                        nearestZ = -z;
                    } else {
                        nearestZ = z;
                    }

                    if ((decor->type & 0x100) != 0 && nearestZ >= nearestX) {
                        int drawX = x + WALL_DECORATION_INSET_X[rotation];
                        int drawZ = z + WALL_DECORATION_INSET_Z[rotation];
                        model_draw(decor->model, rotation * 512 + 256, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, drawX, y, drawZ, decor->bitset);
                    }

                    if ((decor->type & 0x200) != 0 && nearestZ <= nearestX) {
                        int drawX = x + WALL_DECORATION_OUTSET_X[rotation];
                        int drawZ = z + WALL_DECORATION_OUTSET_Z[rotation];
                        model_draw(decor->model, rotation * 512 + 1280 & 0x7ff, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, drawX, y, drawZ, decor->bitset);
                    }
                }
            }

            Wall *wall = tile->wall;
            if (wall) {
                if ((wall->typeB & tile->backWallTypes) != 0 && !world3d_wall_visible(world3d, occludeLevel, tileX, tileZ, wall->typeB)) {
                    model_draw(wall->modelB, 0, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, wall->x - _World3D.eyeX, wall->y - _World3D.eyeY, wall->z - _World3D.eyeZ, wall->bitset);
                }

                if ((wall->typeA & tile->backWallTypes) != 0 && !world3d_wall_visible(world3d, occludeLevel, tileX, tileZ, wall->typeA)) {
                    model_draw(wall->modelA, 0, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, wall->x - _World3D.eyeX, wall->y - _World3D.eyeY, wall->z - _World3D.eyeZ, wall->bitset);
                }
            }
        }

        if (level < world3d->maxLevel - 1) {
            Ground *above = world3d->levelTiles[level + 1][tileX][tileZ];
            if (above && above->update) {
                linklist_add_tail(_World3D.drawTileQueue, &above->link);
            }
        }

        if (tileX < _World3D.eyeTileX) {
            Ground *adjacent = tiles[tileX + 1][tileZ];
            if (adjacent && adjacent->update) {
                linklist_add_tail(_World3D.drawTileQueue, &adjacent->link);
            }
        }

        if (tileZ < _World3D.eyeTileZ) {
            Ground *adjacent = tiles[tileX][tileZ + 1];
            if (adjacent && adjacent->update) {
                linklist_add_tail(_World3D.drawTileQueue, &adjacent->link);
            }
        }

        if (tileX > _World3D.eyeTileX) {
            Ground *adjacent = tiles[tileX - 1][tileZ];
            if (adjacent && adjacent->update) {
                linklist_add_tail(_World3D.drawTileQueue, &adjacent->link);
            }
        }

        if (tileZ > _World3D.eyeTileZ) {
            Ground *adjacent = tiles[tileX][tileZ - 1];
            if (adjacent && adjacent->update) {
                linklist_add_tail(_World3D.drawTileQueue, &adjacent->link);
            }
        }
    }
}

void world3d_draw_tileunderlay(World3D *world3d, TileUnderlay *underlay, int level, int tileX, int tileZ, int sinEyePitch, int cosEyePitch, int sinEyeYaw, int cosEyeYaw) {
    int x3;
    int x0 = x3 = (tileX << 7) - _World3D.eyeX;
    int z1;
    int z0 = z1 = (tileZ << 7) - _World3D.eyeZ;
    int x2;
    int x1 = x2 = x0 + 128;
    int z3;
    int z2 = z3 = z0 + 128;

    int y0 = world3d->levelHeightmaps[level][tileX][tileZ] - _World3D.eyeY;
    int y1 = world3d->levelHeightmaps[level][tileX + 1][tileZ] - _World3D.eyeY;
    int y2 = world3d->levelHeightmaps[level][tileX + 1][tileZ + 1] - _World3D.eyeY;
    int y3 = world3d->levelHeightmaps[level][tileX][tileZ + 1] - _World3D.eyeY;

    int tmp = (z0 * sinEyeYaw + x0 * cosEyeYaw) >> 16;
    z0 = (z0 * cosEyeYaw - x0 * sinEyeYaw) >> 16;
    x0 = tmp;

    tmp = (y0 * cosEyePitch - z0 * sinEyePitch) >> 16;
    z0 = (y0 * sinEyePitch + z0 * cosEyePitch) >> 16;
    y0 = tmp;

    if (z0 < 50) {
        return;
    }

    tmp = (z1 * sinEyeYaw + x1 * cosEyeYaw) >> 16;
    z1 = (z1 * cosEyeYaw - x1 * sinEyeYaw) >> 16;
    x1 = tmp;

    tmp = (y1 * cosEyePitch - z1 * sinEyePitch) >> 16;
    z1 = (y1 * sinEyePitch + z1 * cosEyePitch) >> 16;
    y1 = tmp;

    if (z1 < 50) {
        return;
    }

    tmp = (z2 * sinEyeYaw + x2 * cosEyeYaw) >> 16;
    z2 = (z2 * cosEyeYaw - x2 * sinEyeYaw) >> 16;
    x2 = tmp;

    tmp = (y2 * cosEyePitch - z2 * sinEyePitch) >> 16;
    z2 = (y2 * sinEyePitch + z2 * cosEyePitch) >> 16;
    y2 = tmp;

    if (z2 < 50) {
        return;
    }

    tmp = (z3 * sinEyeYaw + x3 * cosEyeYaw) >> 16;
    z3 = (z3 * cosEyeYaw - x3 * sinEyeYaw) >> 16;
    x3 = tmp;

    tmp = (y3 * cosEyePitch - z3 * sinEyePitch) >> 16;
    z3 = (y3 * sinEyePitch + z3 * cosEyePitch) >> 16;
    y3 = tmp;

    if (z3 < 50) {
        return;
    }

    int px0 = _Pix3D.center_x + (x0 << 9) / z0;
    int py0 = _Pix3D.center_y + (y0 << 9) / z0;
    int pz0 = _Pix3D.center_x + (x1 << 9) / z1;
    int px1 = _Pix3D.center_y + (y1 << 9) / z1;
    int py1 = _Pix3D.center_x + (x2 << 9) / z2;
    int pz1 = _Pix3D.center_y + (y2 << 9) / z2;
    int px3 = _Pix3D.center_x + (x3 << 9) / z3;
    int py3 = _Pix3D.center_y + (y3 << 9) / z3;

    _Pix3D.alpha = 0;

    if ((py1 - px3) * (px1 - py3) - (pz1 - py3) * (pz0 - px3) > 0) {
        _Pix3D.clipX = py1 < 0 || px3 < 0 || pz0 < 0 || py1 > _Pix2D.bound_x || px3 > _Pix2D.bound_x || pz0 > _Pix2D.bound_x;
        if (_World3D.takingInput && point_inside_triangle(_World3D.mouseX, _World3D.mouseY, pz1, py3, px1, py1, px3, pz0)) {
            _World3D.clickTileX = tileX;
            _World3D.clickTileZ = tileZ;
        }
        if (underlay->textureId == -1) {
            if (underlay->northeastColor != 12345678) {
                gouraudTriangle(py1, px3, pz0, pz1, py3, px1, underlay->northeastColor, underlay->northwestColor, underlay->southeastColor);
            }
        } else if (_World3D.lowMemory) {
            int averageColor = TEXTURE_HSL[underlay->textureId];
            gouraudTriangle(py1, px3, pz0, pz1, py3, px1, mul_lightness(averageColor, underlay->northeastColor), mul_lightness(averageColor, underlay->northwestColor), mul_lightness(averageColor, underlay->southeastColor));
        } else if (underlay->flat) {
            textureTriangle(py1, px3, pz0, pz1, py3, px1, underlay->northeastColor, underlay->northwestColor, underlay->southeastColor, x0, y0, z0, x1, x3, y1, y3, z1, z3, underlay->textureId);
        } else {
            textureTriangle(py1, px3, pz0, pz1, py3, px1, underlay->northeastColor, underlay->northwestColor, underlay->southeastColor, x2, y2, z2, x3, x1, y3, y1, z3, z1, underlay->textureId);
        }
    }
    if ((px0 - pz0) * (py3 - px1) - (py0 - px1) * (px3 - pz0) <= 0) {
        return;
    }
    _Pix3D.clipX = px0 < 0 || pz0 < 0 || px3 < 0 || px0 > _Pix2D.bound_x || pz0 > _Pix2D.bound_x || px3 > _Pix2D.bound_x;
    if (_World3D.takingInput && point_inside_triangle(_World3D.mouseX, _World3D.mouseY, py0, px1, py3, px0, pz0, px3)) {
        _World3D.clickTileX = tileX;
        _World3D.clickTileZ = tileZ;
    }
    if (underlay->textureId != -1) {
        if (!_World3D.lowMemory) {
            textureTriangle(px0, pz0, px3, py0, px1, py3, underlay->southwestColor, underlay->southeastColor, underlay->northwestColor, x0, y0, z0, x1, x3, y1, y3, z1, z3, underlay->textureId);
            return;
        }
        int averageColor = TEXTURE_HSL[underlay->textureId];
        gouraudTriangle(px0, pz0, px3, py0, px1, py3, mul_lightness(averageColor, underlay->southwestColor), mul_lightness(averageColor, underlay->southeastColor), mul_lightness(averageColor, underlay->northwestColor));
    } else if (underlay->southwestColor != 12345678) {
        gouraudTriangle(px0, pz0, px3, py0, px1, py3, underlay->southwestColor, underlay->southeastColor, underlay->northwestColor);
    }
}

void world3d_draw_tileoverlay(int tileX, int tileZ, TileOverlay *overlay, int sinEyePitch, int cosEyePitch, int sinEyeYaw, int cosEyeYaw) {
    int vertexCount = overlay->vertexCount;

    for (int i = 0; i < vertexCount; i++) {
        int x = overlay->vertexX[i] - _World3D.eyeX;
        int y = overlay->vertexY[i] - _World3D.eyeY;
        int z = overlay->vertexZ[i] - _World3D.eyeZ;

        int tmp = (z * sinEyeYaw + x * cosEyeYaw) >> 16;
        z = (z * cosEyeYaw - x * sinEyeYaw) >> 16;
        x = tmp;

        tmp = (y * cosEyePitch - z * sinEyePitch) >> 16;
        z = (y * sinEyePitch + z * cosEyePitch) >> 16;
        y = tmp;

        if (z < 50) {
            return;
        }

        if (overlay->triangleTextureIds) {
            _TileOverlay.tmpViewspaceX[i] = x;
            _TileOverlay.tmpViewspaceY[i] = y;
            _TileOverlay.tmpViewspaceZ[i] = z;
        }
        _TileOverlay.tmpScreenX[i] = _Pix3D.center_x + (x << 9) / z;
        _TileOverlay.tmpScreenY[i] = _Pix3D.center_y + (y << 9) / z;
    }

    _Pix3D.alpha = 0;

    vertexCount = overlay->triangleCount;
    for (int v = 0; v < vertexCount; v++) {
        int a = overlay->triangleVertexA[v];
        int b = overlay->triangleVertexB[v];
        int c = overlay->triangleVertexC[v];

        int x0 = _TileOverlay.tmpScreenX[a];
        int x1 = _TileOverlay.tmpScreenX[b];
        int x2 = _TileOverlay.tmpScreenX[c];
        int y0 = _TileOverlay.tmpScreenY[a];
        int y1 = _TileOverlay.tmpScreenY[b];
        int y2 = _TileOverlay.tmpScreenY[c];

        if ((x0 - x1) * (y2 - y1) - (y0 - y1) * (x2 - x1) > 0) {
            _Pix3D.clipX = x0 < 0 || x1 < 0 || x2 < 0 || x0 > _Pix2D.bound_x || x1 > _Pix2D.bound_x || x2 > _Pix2D.bound_x;
            if (_World3D.takingInput && point_inside_triangle(_World3D.mouseX, _World3D.mouseY, y0, y1, y2, x0, x1, x2)) {
                _World3D.clickTileX = tileX;
                _World3D.clickTileZ = tileZ;
            }
            if (!overlay->triangleTextureIds || overlay->triangleTextureIds[v] == -1) {
                if (overlay->triangleColorA[v] != 12345678) {
                    gouraudTriangle(x0, x1, x2, y0, y1, y2, overlay->triangleColorA[v], overlay->triangleColorB[v], overlay->triangleColorC[v]);
                }
            } else if (_World3D.lowMemory) {
                int textureColor = TEXTURE_HSL[overlay->triangleTextureIds[v]];
                gouraudTriangle(x0, x1, x2, y0, y1, y2, mul_lightness(textureColor, overlay->triangleColorA[v]), mul_lightness(textureColor, overlay->triangleColorB[v]), mul_lightness(textureColor, overlay->triangleColorC[v]));
            } else if (overlay->flat) {
                textureTriangle(x0, x1, x2, y0, y1, y2, overlay->triangleColorA[v], overlay->triangleColorB[v], overlay->triangleColorC[v], _TileOverlay.tmpViewspaceX[0], _TileOverlay.tmpViewspaceY[0], _TileOverlay.tmpViewspaceZ[0], _TileOverlay.tmpViewspaceX[1], _TileOverlay.tmpViewspaceX[3], _TileOverlay.tmpViewspaceY[1], _TileOverlay.tmpViewspaceY[3], _TileOverlay.tmpViewspaceZ[1], _TileOverlay.tmpViewspaceZ[3], overlay->triangleTextureIds[v]);
            } else {
                textureTriangle(x0, x1, x2, y0, y1, y2, overlay->triangleColorA[v], overlay->triangleColorB[v], overlay->triangleColorC[v], _TileOverlay.tmpViewspaceX[a], _TileOverlay.tmpViewspaceY[a], _TileOverlay.tmpViewspaceZ[a], _TileOverlay.tmpViewspaceX[b], _TileOverlay.tmpViewspaceX[c], _TileOverlay.tmpViewspaceY[b], _TileOverlay.tmpViewspaceY[c], _TileOverlay.tmpViewspaceZ[b], _TileOverlay.tmpViewspaceZ[c], overlay->triangleTextureIds[v]);
            }
        }
    }
}

int mul_lightness(int hsl, int lightness) {
    int invLightness = 127 - lightness;
    lightness = invLightness * (hsl & 0x7f) / 160;
    if (lightness < 2) {
        lightness = 2;
    } else if (lightness > 126) {
        lightness = 126;
    }
    return (hsl & 0xff80) + lightness;
}

bool point_inside_triangle(int x, int y, int y0, int y1, int y2, int x0, int x1, int x2) {
    if (y < y0 && y < y1 && y < y2) {
        return false;
    } else if (y > y0 && y > y1 && y > y2) {
        return false;
    } else if (x < x0 && x < x1 && x < x2) {
        return false;
    } else if (x > x0 && x > x1 && x > x2) {
        return false;
    } else {
        int crossProduct_01 = (y - y0) * (x1 - x0) - (x - x0) * (y1 - y0);
        int crossProduct_20 = (y - y2) * (x0 - x2) - (x - x2) * (y0 - y2);
        int crossProduct_12 = (y - y1) * (x2 - x1) - (x - x1) * (y2 - y1);
        return crossProduct_01 * crossProduct_12 > 0 && crossProduct_12 * crossProduct_20 > 0;
    }
}

void world3d_update_activeoccluders(void) {
    int count = _World3D.levelOccluderCount[_World3D.topLevel];
    Occlude **occluders = _World3D.levelOccluders[_World3D.topLevel];
    _World3D.activeOccluderCount = 0;
    for (int i = 0; i < count; i++) {
        Occlude *occluder = occluders[i];
        int deltaMaxY;
        int deltaMinTileZ;
        int deltaMaxTileZ;
        int deltaMaxTileX;
        if (occluder->type == 1) {
            deltaMaxY = occluder->minTileX + 25 - _World3D.eyeTileX;
            if (deltaMaxY >= 0 && deltaMaxY <= 50) {
                deltaMinTileZ = occluder->minTileZ + 25 - _World3D.eyeTileZ;
                if (deltaMinTileZ < 0) {
                    deltaMinTileZ = 0;
                }
                deltaMaxTileZ = occluder->maxTileZ + 25 - _World3D.eyeTileZ;
                if (deltaMaxTileZ > 50) {
                    deltaMaxTileZ = 50;
                }
                bool ok = false;
                while (deltaMinTileZ <= deltaMaxTileZ) {
                    if (_World3D.visibilityMap[deltaMaxY][deltaMinTileZ++]) {
                        ok = true;
                        break;
                    }
                }
                if (ok) {
                    deltaMaxTileX = _World3D.eyeX - occluder->minX;
                    if (deltaMaxTileX > 32) {
                        occluder->mode = 1;
                    } else {
                        if (deltaMaxTileX >= -32) {
                            continue;
                        }
                        occluder->mode = 2;
                        deltaMaxTileX = -deltaMaxTileX;
                    }
                    occluder->minDeltaZ = ((occluder->minZ - _World3D.eyeZ) << 8) / deltaMaxTileX;
                    occluder->maxDeltaZ = ((occluder->maxZ - _World3D.eyeZ) << 8) / deltaMaxTileX;
                    occluder->minDeltaY = ((occluder->minY - _World3D.eyeY) << 8) / deltaMaxTileX;
                    occluder->maxDeltaY = ((occluder->maxY - _World3D.eyeY) << 8) / deltaMaxTileX;
                    _World3D.activeOccluders[_World3D.activeOccluderCount++] = occluder;
                }
            }
        } else if (occluder->type == 2) {
            deltaMaxY = occluder->minTileZ + 25 - _World3D.eyeTileZ;
            if (deltaMaxY >= 0 && deltaMaxY <= 50) {
                deltaMinTileZ = occluder->minTileX + 25 - _World3D.eyeTileX;
                if (deltaMinTileZ < 0) {
                    deltaMinTileZ = 0;
                }
                deltaMaxTileZ = occluder->maxTileX + 25 - _World3D.eyeTileX;
                if (deltaMaxTileZ > 50) {
                    deltaMaxTileZ = 50;
                }
                bool ok = false;
                while (deltaMinTileZ <= deltaMaxTileZ) {
                    if (_World3D.visibilityMap[deltaMinTileZ++][deltaMaxY]) {
                        ok = true;
                        break;
                    }
                }
                if (ok) {
                    deltaMaxTileX = _World3D.eyeZ - occluder->minZ;
                    if (deltaMaxTileX > 32) {
                        occluder->mode = 3;
                    } else {
                        if (deltaMaxTileX >= -32) {
                            continue;
                        }
                        occluder->mode = 4;
                        deltaMaxTileX = -deltaMaxTileX;
                    }
                    occluder->minDeltaX = ((occluder->minX - _World3D.eyeX) << 8) / deltaMaxTileX;
                    occluder->maxDeltaX = ((occluder->maxX - _World3D.eyeX) << 8) / deltaMaxTileX;
                    occluder->minDeltaY = ((occluder->minY - _World3D.eyeY) << 8) / deltaMaxTileX;
                    occluder->maxDeltaY = ((occluder->maxY - _World3D.eyeY) << 8) / deltaMaxTileX;
                    _World3D.activeOccluders[_World3D.activeOccluderCount++] = occluder;
                }
            }
        } else if (occluder->type == 4) {
            deltaMaxY = occluder->minY - _World3D.eyeY;
            if (deltaMaxY > 128) {
                deltaMinTileZ = occluder->minTileZ + 25 - _World3D.eyeTileZ;
                if (deltaMinTileZ < 0) {
                    deltaMinTileZ = 0;
                }
                deltaMaxTileZ = occluder->maxTileZ + 25 - _World3D.eyeTileZ;
                if (deltaMaxTileZ > 50) {
                    deltaMaxTileZ = 50;
                }
                if (deltaMinTileZ <= deltaMaxTileZ) {
                    int deltaMinTileX = occluder->minTileX + 25 - _World3D.eyeTileX;
                    if (deltaMinTileX < 0) {
                        deltaMinTileX = 0;
                    }
                    deltaMaxTileX = occluder->maxTileX + 25 - _World3D.eyeTileX;
                    if (deltaMaxTileX > 50) {
                        deltaMaxTileX = 50;
                    }
                    bool ok = false;
                    for (int x = deltaMinTileX; x <= deltaMaxTileX; x++) {
                        for (int z = deltaMinTileZ; z <= deltaMaxTileZ; z++) {
                            if (_World3D.visibilityMap[x][z]) {
                                ok = true;
                                goto visible_tile_found;
                            }
                        }
                    }
                visible_tile_found:
                    if (ok) {
                        occluder->mode = 5;
                        occluder->minDeltaX = ((occluder->minX - _World3D.eyeX) << 8) / deltaMaxY;
                        occluder->maxDeltaX = ((occluder->maxX - _World3D.eyeX) << 8) / deltaMaxY;
                        occluder->minDeltaZ = ((occluder->minZ - _World3D.eyeZ) << 8) / deltaMaxY;
                        occluder->maxDeltaZ = ((occluder->maxZ - _World3D.eyeZ) << 8) / deltaMaxY;
                        _World3D.activeOccluders[_World3D.activeOccluderCount++] = occluder;
                    }
                }
            }
        }
    }
}

bool world3d_tile_visible(World3D *world3d, int level, int x, int z) {
    int cycle = world3d->levelTileOcclusionCycles[level][x][z];
    if (cycle == -_World3D.cycle) {
        return false;
    } else if (cycle == _World3D.cycle) {
        return true;
    } else {
        int sx = x << 7;
        int sz = z << 7;
        if (world3d_occluded(sx + 1, world3d->levelHeightmaps[level][x][z], sz + 1) && world3d_occluded(sx + 128 - 1, world3d->levelHeightmaps[level][x + 1][z], sz + 1) && world3d_occluded(sx + 128 - 1, world3d->levelHeightmaps[level][x + 1][z + 1], sz + 128 - 1) && world3d_occluded(sx + 1, world3d->levelHeightmaps[level][x][z + 1], sz + 128 - 1)) {
            world3d->levelTileOcclusionCycles[level][x][z] = _World3D.cycle;
            return true;
        } else {
            world3d->levelTileOcclusionCycles[level][x][z] = -_World3D.cycle;
            return false;
        }
    }
}

bool world3d_wall_visible(World3D *world3d, int level, int x, int z, int type) {
    if (!world3d_tile_visible(world3d, level, x, z)) {
        return false;
    }
    int sceneX = x << 7;
    int sceneZ = z << 7;
    int sceneY = world3d->levelHeightmaps[level][x][z] - 1;
    int y0 = sceneY - 120;
    int y1 = sceneY - 230;
    int y2 = sceneY - 238;
    if (type < 16) {
        if (type == 1) {
            if (sceneX > _World3D.eyeX) {
                if (!world3d_occluded(sceneX, sceneY, sceneZ)) {
                    return false;
                }
                if (!world3d_occluded(sceneX, sceneY, sceneZ + 128)) {
                    return false;
                }
            }
            if (level > 0) {
                if (!world3d_occluded(sceneX, y0, sceneZ)) {
                    return false;
                }
                if (!world3d_occluded(sceneX, y0, sceneZ + 128)) {
                    return false;
                }
            }
            if (!world3d_occluded(sceneX, y1, sceneZ)) {
                return false;
            }
            return world3d_occluded(sceneX, y1, sceneZ + 128);
        }
        if (type == 2) {
            if (sceneZ < _World3D.eyeZ) {
                if (!world3d_occluded(sceneX, sceneY, sceneZ + 128)) {
                    return false;
                }
                if (!world3d_occluded(sceneX + 128, sceneY, sceneZ + 128)) {
                    return false;
                }
            }
            if (level > 0) {
                if (!world3d_occluded(sceneX, y0, sceneZ + 128)) {
                    return false;
                }
                if (!world3d_occluded(sceneX + 128, y0, sceneZ + 128)) {
                    return false;
                }
            }
            if (!world3d_occluded(sceneX, y1, sceneZ + 128)) {
                return false;
            }
            return world3d_occluded(sceneX + 128, y1, sceneZ + 128);
        }
        if (type == 4) {
            if (sceneX < _World3D.eyeX) {
                if (!world3d_occluded(sceneX + 128, sceneY, sceneZ)) {
                    return false;
                }
                if (!world3d_occluded(sceneX + 128, sceneY, sceneZ + 128)) {
                    return false;
                }
            }
            if (level > 0) {
                if (!world3d_occluded(sceneX + 128, y0, sceneZ)) {
                    return false;
                }
                if (!world3d_occluded(sceneX + 128, y0, sceneZ + 128)) {
                    return false;
                }
            }
            if (!world3d_occluded(sceneX + 128, y1, sceneZ)) {
                return false;
            }
            return world3d_occluded(sceneX + 128, y1, sceneZ + 128);
        }
        if (type == 8) {
            if (sceneZ > _World3D.eyeZ) {
                if (!world3d_occluded(sceneX, sceneY, sceneZ)) {
                    return false;
                }
                if (!world3d_occluded(sceneX + 128, sceneY, sceneZ)) {
                    return false;
                }
            }
            if (level > 0) {
                if (!world3d_occluded(sceneX, y0, sceneZ)) {
                    return false;
                }
                if (!world3d_occluded(sceneX + 128, y0, sceneZ)) {
                    return false;
                }
            }
            if (!world3d_occluded(sceneX, y1, sceneZ)) {
                return false;
            }
            return world3d_occluded(sceneX + 128, y1, sceneZ);
        }
    }
    if (!world3d_occluded(sceneX + 64, y2, sceneZ + 64)) {
        return false;
    } else if (type == 16) {
        return world3d_occluded(sceneX, y1, sceneZ + 128);
    } else if (type == 32) {
        return world3d_occluded(sceneX + 128, y1, sceneZ + 128);
    } else if (type == 64) {
        return world3d_occluded(sceneX + 128, y1, sceneZ);
    } else if (type == 128) {
        return world3d_occluded(sceneX, y1, sceneZ);
    } else {
        rs2_error("Warning unsupported wall type\n");
        return true;
    }
}

bool world3d_visible(World3D *world3d, int level, int tileX, int tileZ, int y) {
    if (world3d_tile_visible(world3d, level, tileX, tileZ)) {
        int x = tileX << 7;
        int z = tileZ << 7;
        return world3d_occluded(x + 1, world3d->levelHeightmaps[level][tileX][tileZ] - y, z + 1) && world3d_occluded(x + 128 - 1, world3d->levelHeightmaps[level][tileX + 1][tileZ] - y, z + 1) && world3d_occluded(x + 128 - 1, world3d->levelHeightmaps[level][tileX + 1][tileZ + 1] - y, z + 128 - 1) && world3d_occluded(x + 1, world3d->levelHeightmaps[level][tileX][tileZ + 1] - y, z + 128 - 1);
    } else {
        return false;
    }
}

bool world3d_loc_visible(World3D *world3d, int level, int minX, int maxX, int minZ, int maxZ, int y) {
    int x;
    int z;
    if (minX != maxX || minZ != maxZ) {
        for (x = minX; x <= maxX; x++) {
            for (z = minZ; z <= maxZ; z++) {
                if (world3d->levelTileOcclusionCycles[level][x][z] == -_World3D.cycle) {
                    return false;
                }
            }
        }
        z = (minX << 7) + 1;
        int z0 = (minZ << 7) + 2;
        int y0 = world3d->levelHeightmaps[level][minX][minZ] - y;
        if (!world3d_occluded(z, y0, z0)) {
            return false;
        }
        int x1 = (maxX << 7) - 1;
        if (!world3d_occluded(x1, y0, z0)) {
            return false;
        }
        int z1 = (maxZ << 7) - 1;
        if (!world3d_occluded(z, y0, z1)) {
            return false;
        } else
            return world3d_occluded(x1, y0, z1);
    } else if (world3d_tile_visible(world3d, level, minX, minZ)) {
        x = minX << 7;
        z = minZ << 7;
        return world3d_occluded(x + 1, world3d->levelHeightmaps[level][minX][minZ] - y, z + 1) && world3d_occluded(x + 128 - 1, world3d->levelHeightmaps[level][minX + 1][minZ] - y, z + 1) && world3d_occluded(x + 128 - 1, world3d->levelHeightmaps[level][minX + 1][minZ + 1] - y, z + 128 - 1) && world3d_occluded(x + 1, world3d->levelHeightmaps[level][minX][minZ + 1] - y, z + 128 - 1);
    } else {
        return false;
    }
}

bool world3d_occluded(int x, int y, int z) {
    for (int i = 0; i < _World3D.activeOccluderCount; i++) {
        Occlude *occluder = _World3D.activeOccluders[i];

        if (occluder->mode == 1) {
            int dx = occluder->minX - x;
            if (dx > 0) {
                int minZ = occluder->minZ + (occluder->minDeltaZ * dx >> 8);
                int maxZ = occluder->maxZ + (occluder->maxDeltaZ * dx >> 8);
                int minY = occluder->minY + (occluder->minDeltaY * dx >> 8);
                int maxY = occluder->maxY + (occluder->maxDeltaY * dx >> 8);
                if (z >= minZ && z <= maxZ && y >= minY && y <= maxY) {
                    return true;
                }
            }
        } else if (occluder->mode == 2) {
            int dx = x - occluder->minX;
            if (dx > 0) {
                int minZ = occluder->minZ + (occluder->minDeltaZ * dx >> 8);
                int maxZ = occluder->maxZ + (occluder->maxDeltaZ * dx >> 8);
                int minY = occluder->minY + (occluder->minDeltaY * dx >> 8);
                int maxY = occluder->maxY + (occluder->maxDeltaY * dx >> 8);
                if (z >= minZ && z <= maxZ && y >= minY && y <= maxY) {
                    return true;
                }
            }
        } else if (occluder->mode == 3) {
            int dz = occluder->minZ - z;
            if (dz > 0) {
                int minX = occluder->minX + (occluder->minDeltaX * dz >> 8);
                int maxX = occluder->maxX + (occluder->maxDeltaX * dz >> 8);
                int minY = occluder->minY + (occluder->minDeltaY * dz >> 8);
                int maxY = occluder->maxY + (occluder->maxDeltaY * dz >> 8);
                if (x >= minX && x <= maxX && y >= minY && y <= maxY) {
                    return true;
                }
            }
        } else if (occluder->mode == 4) {
            int dz = z - occluder->minZ;
            if (dz > 0) {
                int minX = occluder->minX + (occluder->minDeltaX * dz >> 8);
                int maxX = occluder->maxX + (occluder->maxDeltaX * dz >> 8);
                int minY = occluder->minY + (occluder->minDeltaY * dz >> 8);
                int maxY = occluder->maxY + (occluder->maxDeltaY * dz >> 8);
                if (x >= minX && x <= maxX && y >= minY && y <= maxY) {
                    return true;
                }
            }
        } else if (occluder->mode == 5) {
            int dy = y - occluder->minY;
            if (dy > 0) {
                int minX = occluder->minX + (occluder->minDeltaX * dy >> 8);
                int maxX = occluder->maxX + (occluder->maxDeltaX * dy >> 8);
                int minZ = occluder->minZ + (occluder->minDeltaZ * dy >> 8);
                int maxZ = occluder->maxZ + (occluder->maxDeltaZ * dy >> 8);
                if (x >= minX && x <= maxX && z >= minZ && z <= maxZ) {
                    return true;
                }
            }
        }
    }
    return false;
}
