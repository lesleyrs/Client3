#pragma once

#include <stdbool.h>

typedef struct {
    int offsetX;
    int offsetZ;
    int sizeX;
    int sizeZ;
    int **flags;
} CollisionMap;

CollisionMap *collisionmap_new(int sizeX, int sizeZ);
void collisionmap_reset(CollisionMap *map);
void collisionmap_add_wall(CollisionMap *map, int tileX, int tileZ, int shape, int rotation, bool blockrange);
void collisionmap_add_loc(CollisionMap *map, int tileX, int tileZ, int sizeX, int sizeZ, int rotation, bool blockrange);
void collisionmap_set_blocked(CollisionMap *map, int tileX, int tileZ);
void collisionmap_add_cmap(CollisionMap *map, int x, int z, int flags);
void collisionmap_del_wall(CollisionMap *map, int tileX, int tileZ, int shape, int rotation, bool blockrange);
void collisionmap_del_loc(CollisionMap *map, int tileX, int tileZ, int sizeX, int sizeZ, int rotation, bool blockrange);
void collisionmap_rem_cmap(CollisionMap *map, int x, int z, int flags);
void collisionmap_remove_blocked(CollisionMap *map, int tileX, int tileZ);
bool collisionmap_test_wall(CollisionMap *map, int sourceX, int sourceZ, int destX, int destZ, int shape, int rotation);
bool collisionmap_test_wdecor(CollisionMap *map, int sourceX, int sourceZ, int destX, int destZ, int shape, int rotation);
bool collisionmap_test_loc(CollisionMap *map, int srcX, int srcZ, int dstX, int dstZ, int dstSizeX, int dstSizeZ, int forceapproach);
void collisionmap_free(CollisionMap *map);
