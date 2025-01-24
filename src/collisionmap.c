#include <stdlib.h>

#include "collisionmap.h"
#include "loctype.h"

CollisionMap *collisionmap_new(int sizeX, int sizeZ) {
    CollisionMap *map = calloc(1, sizeof(CollisionMap));
    map->offsetX = 0;
    map->offsetZ = 0;
    map->sizeX = sizeX;
    map->sizeZ = sizeZ;
    map->flags = calloc(map->sizeX, sizeof(int *));
    for (int i = 0; i < map->sizeX; i++) {
        map->flags[i] = calloc(map->sizeZ, sizeof(int));
    }
    collisionmap_reset(map);
    return map;
}

void collisionmap_free(CollisionMap *map) {
    for (int i = 0; i < map->sizeX; i++) {
        free(map->flags[i]);
    }
    free(map->flags);
    free(map);
}

void collisionmap_reset(CollisionMap *map) {
    for (int x = 0; x < map->sizeX; x++) {
        for (int z = 0; z < map->sizeZ; z++) {
            if (x == 0 || z == 0 || x == map->sizeX - 1 || z == map->sizeZ - 1) {
                map->flags[x][z] = 0xffffff;
            } else {
                map->flags[x][z] = 0;
            }
        }
    }
}

void collisionmap_add_wall(CollisionMap *map, int tileX, int tileZ, int shape, int rotation, bool blockrange) {
    int x = tileX - map->offsetX;
    int z = tileZ - map->offsetZ;

    if (shape == WALL_STRAIGHT) {
        if (rotation == 0) {
            collisionmap_add_cmap(map, x, z, 0x80);
            collisionmap_add_cmap(map, x - 1, z, 0x8);
        } else if (rotation == 1) {
            collisionmap_add_cmap(map, x, z, 0x2);
            collisionmap_add_cmap(map, x, z + 1, 0x20);
        } else if (rotation == 2) {
            collisionmap_add_cmap(map, x, z, 0x8);
            collisionmap_add_cmap(map, x + 1, z, 0x80);
        } else if (rotation == 3) {
            collisionmap_add_cmap(map, x, z, 32);
            collisionmap_add_cmap(map, x, z - 1, 0x2);
        }
    } else if (shape == WALL_DIAGONALCORNER || shape == WALL_SQUARECORNER) {
        if (rotation == 0) {
            collisionmap_add_cmap(map, x, z, 0x1);
            collisionmap_add_cmap(map, x - 1, z + 1, 0x10);
        } else if (rotation == 1) {
            collisionmap_add_cmap(map, x, z, 0x4);
            collisionmap_add_cmap(map, x + 1, z + 1, 0x40);
        } else if (rotation == 2) {
            collisionmap_add_cmap(map, x, z, 0x10);
            collisionmap_add_cmap(map, x + 1, z - 1, 0x1);
        } else if (rotation == 3) {
            collisionmap_add_cmap(map, x, z, 64);
            collisionmap_add_cmap(map, x - 1, z - 1, 0x4);
        }
    } else if (shape == WALL_L) {
        if (rotation == 0) {
            collisionmap_add_cmap(map, x, z, 0x82);
            collisionmap_add_cmap(map, x - 1, z, 0x8);
            collisionmap_add_cmap(map, x, z + 1, 0x20);
        } else if (rotation == 1) {
            collisionmap_add_cmap(map, x, z, 0xa);
            collisionmap_add_cmap(map, x, z + 1, 0x20);
            collisionmap_add_cmap(map, x + 1, z, 0x80);
        } else if (rotation == 2) {
            collisionmap_add_cmap(map, x, z, 0x28);
            collisionmap_add_cmap(map, x + 1, z, 0x80);
            collisionmap_add_cmap(map, x, z - 1, 0x2);
        } else if (rotation == 3) {
            collisionmap_add_cmap(map, x, z, 0xa0);
            collisionmap_add_cmap(map, x, z - 1, 0x2);
            collisionmap_add_cmap(map, x - 1, z, 0x8);
        }
    }

    if (blockrange) {
        if (shape == WALL_STRAIGHT) {
            if (rotation == 0) {
                collisionmap_add_cmap(map, x, z, 0x10000);
                collisionmap_add_cmap(map, x - 1, z, 0x1000);
            } else if (rotation == 1) {
                collisionmap_add_cmap(map, x, z, 0x400);
                collisionmap_add_cmap(map, x, z + 1, 0x4000);
            } else if (rotation == 2) {
                collisionmap_add_cmap(map, x, z, 0x1000);
                collisionmap_add_cmap(map, x + 1, z, 0x10000);
            } else if (rotation == 3) {
                collisionmap_add_cmap(map, x, z, 0x4000);
                collisionmap_add_cmap(map, x, z - 1, 0x400);
            }
        } else if (shape == WALL_DIAGONAL || shape == WALL_SQUARECORNER) {
            if (rotation == 0) {
                collisionmap_add_cmap(map, x, z, 0x200);
                collisionmap_add_cmap(map, x - 1, z + 1, 0x2000);
            } else if (rotation == 1) {
                collisionmap_add_cmap(map, x, z, 0x800);
                collisionmap_add_cmap(map, x + 1, z + 1, 0x8000);
            } else if (rotation == 2) {
                collisionmap_add_cmap(map, x, z, 0x2000);
                collisionmap_add_cmap(map, x + 1, z - 1, 0x200);
            } else if (rotation == 3) {
                collisionmap_add_cmap(map, x, z, 0x8000);
                collisionmap_add_cmap(map, x - 1, z - 1, 0x800);
            }
        } else if (shape == WALL_L) {
            if (rotation == 0) {
                collisionmap_add_cmap(map, x, z, 0x10400);
                collisionmap_add_cmap(map, x - 1, z, 0x1000);
                collisionmap_add_cmap(map, x, z + 1, 0x4000);
            } else if (rotation == 1) {
                collisionmap_add_cmap(map, x, z, 0x1400);
                collisionmap_add_cmap(map, x, z + 1, 0x4000);
                collisionmap_add_cmap(map, x + 1, z, 0x10000);
            } else if (rotation == 2) {
                collisionmap_add_cmap(map, x, z, 0x5000);
                collisionmap_add_cmap(map, x + 1, z, 0x10000);
                collisionmap_add_cmap(map, x, z - 1, 0x400);
            } else if (rotation == 3) {
                collisionmap_add_cmap(map, x, z, 0x14000);
                collisionmap_add_cmap(map, x, z - 1, 0x400);
                collisionmap_add_cmap(map, x - 1, z, 0x1000);
            }
        }
    }
}

void collisionmap_add_loc(CollisionMap *map, int tileX, int tileZ, int sizeX, int sizeZ, int rotation, bool blockrange) {
    int flags = 0x100;
    if (blockrange) {
        flags += 0x20000;
    }

    int x = tileX - map->offsetX;
    int z = tileZ - map->offsetZ;

    if (rotation == 1 || rotation == 3) {
        int tmp = sizeX;
        sizeX = sizeZ;
        sizeZ = tmp;
    }

    for (int tx = x; tx < x + sizeX; tx++) {
        if (tx < 0 || tx >= map->sizeX) {
            continue;
        }

        for (int tz = z; tz < z + sizeZ; tz++) {
            if (tz < 0 || tz >= map->sizeZ) {
                continue;
            }

            collisionmap_add_cmap(map, tx, tz, flags);
        }
    }
}

void collisionmap_set_blocked(CollisionMap *map, int tileX, int tileZ) {
    int x = tileX - map->offsetX;
    int z = tileZ - map->offsetZ;
    map->flags[x][z] |= 0x200000;
}

void collisionmap_add_cmap(CollisionMap *map, int x, int z, int flags) {
    map->flags[x][z] |= flags;
}

void collisionmap_del_wall(CollisionMap *map, int tileX, int tileZ, int shape, int rotation, bool blockrange) {
    int x = tileX - map->offsetX;
    int z = tileZ - map->offsetZ;

    if (shape == WALL_STRAIGHT) {
        if (rotation == 0) {
            collisionmap_rem_cmap(map, x, z, 0x80);
            collisionmap_rem_cmap(map, x - 1, z, 0x8);
        } else if (rotation == 1) {
            collisionmap_rem_cmap(map, x, z, 0x2);
            collisionmap_rem_cmap(map, x, z + 1, 0x20);
        } else if (rotation == 2) {
            collisionmap_rem_cmap(map, x, z, 0x8);
            collisionmap_rem_cmap(map, x + 1, z, 0x80);
        } else if (rotation == 3) {
            collisionmap_rem_cmap(map, x, z, 0x20);
            collisionmap_rem_cmap(map, x, z - 1, 0x2);
        }
    } else if (shape == WALL_DIAGONALCORNER || shape == WALL_SQUARECORNER) {
        if (rotation == 0) {
            collisionmap_rem_cmap(map, x, z, 0x1);
            collisionmap_rem_cmap(map, x - 1, z + 1, 0x10);
        } else if (rotation == 1) {
            collisionmap_rem_cmap(map, x, z, 0x4);
            collisionmap_rem_cmap(map, x + 1, z + 1, 0x40);
        } else if (rotation == 2) {
            collisionmap_rem_cmap(map, x, z, 0x10);
            collisionmap_rem_cmap(map, x + 1, z - 1, 0x1);
        } else if (rotation == 3) {
            collisionmap_rem_cmap(map, x, z, 0x40);
            collisionmap_rem_cmap(map, x - 1, z - 1, 0x4);
        }
    } else if (shape == WALL_L) {
        if (rotation == 0) {
            collisionmap_rem_cmap(map, x, z, 0x82);
            collisionmap_rem_cmap(map, x - 1, z, 0x8);
            collisionmap_rem_cmap(map, x, z + 1, 0x20);
        } else if (rotation == 1) {
            collisionmap_rem_cmap(map, x, z, 0xa);
            collisionmap_rem_cmap(map, x, z + 1, 0x20);
            collisionmap_rem_cmap(map, x + 1, z, 0x80);
        } else if (rotation == 2) {
            collisionmap_rem_cmap(map, x, z, 0x28);
            collisionmap_rem_cmap(map, x + 1, z, 0x80);
            collisionmap_rem_cmap(map, x, z - 1, 0x2);
        } else if (rotation == 3) {
            collisionmap_rem_cmap(map, x, z, 0xa0);
            collisionmap_rem_cmap(map, x, z - 1, 0x2);
            collisionmap_rem_cmap(map, x - 1, z, 0x8);
        }
    }

    if (blockrange) {
        if (shape == WALL_STRAIGHT) {
            if (rotation == 0) {
                collisionmap_rem_cmap(map, x, z, 0x10000);
                collisionmap_rem_cmap(map, x - 1, z, 0x1000);
            } else if (rotation == 1) {
                collisionmap_rem_cmap(map, x, z, 0x400);
                collisionmap_rem_cmap(map, x, z + 1, 0x4000);
            } else if (rotation == 2) {
                collisionmap_rem_cmap(map, x, z, 0x1000);
                collisionmap_rem_cmap(map, x + 1, z, 0x10000);
            } else if (rotation == 3) {
                collisionmap_rem_cmap(map, x, z, 0x4000);
                collisionmap_rem_cmap(map, x, z - 1, 0x400);
            }
        } else if (shape == WALL_DIAGONALCORNER || shape == WALL_SQUARECORNER) {
            if (rotation == 0) {
                collisionmap_rem_cmap(map, x, z, 0x200);
                collisionmap_rem_cmap(map, x - 1, z + 1, 0x2000);
            } else if (rotation == 1) {
                collisionmap_rem_cmap(map, x, z, 0x800);
                collisionmap_rem_cmap(map, x + 1, z + 1, 0x8000);
            } else if (rotation == 2) {
                collisionmap_rem_cmap(map, x, z, 0x2000);
                collisionmap_rem_cmap(map, x + 1, z - 1, 0x200);
            } else if (rotation == 3) {
                collisionmap_rem_cmap(map, x, z, 0x8000);
                collisionmap_rem_cmap(map, x - 1, z - 1, 0x800);
            }
        } else if (shape == WALL_L) {
            if (rotation == 0) {
                collisionmap_rem_cmap(map, x, z, 0x10400);
                collisionmap_rem_cmap(map, x - 1, z, 0x1000);
                collisionmap_rem_cmap(map, x, z + 1, 0x4000);
            } else if (rotation == 1) {
                collisionmap_rem_cmap(map, x, z, 0x1400);
                collisionmap_rem_cmap(map, x, z + 1, 0x4000);
                collisionmap_rem_cmap(map, x + 1, z, 0x10000);
            } else if (rotation == 2) {
                collisionmap_rem_cmap(map, x, z, 0x5000);
                collisionmap_rem_cmap(map, x + 1, z, 0x10000);
                collisionmap_rem_cmap(map, x, z - 1, 0x400);
            } else if (rotation == 3) {
                collisionmap_rem_cmap(map, x, z, 0x14000);
                collisionmap_rem_cmap(map, x, z - 1, 0x400);
                collisionmap_rem_cmap(map, x - 1, z, 0x1000);
            }
        }
    }
}

void collisionmap_del_loc(CollisionMap *map, int tileX, int tileZ, int sizeX, int sizeZ, int rotation, bool blockrange) {
    int flags = 0x100;
    if (blockrange) {
        flags += 0x20000;
    }

    int x = tileX - map->offsetX;
    int z = tileZ - map->offsetZ;

    if (rotation == 1 || rotation == 3) {
        int tmp = sizeX;
        sizeX = sizeZ;
        sizeZ = tmp;
    }

    for (int tx = x; tx < x + sizeX; tx++) {
        if (tx < 0 || tx >= map->sizeX) {
            continue;
        }

        for (int tz = z; tz < z + sizeZ; tz++) {
            if (tz < 0 || tz >= map->sizeZ) {
                continue;
            }

            collisionmap_rem_cmap(map, tx, tz, flags);
        }
    }
}

void collisionmap_rem_cmap(CollisionMap *map, int x, int z, int flags) {
    map->flags[x][z] &= 0xffffff - flags;
}

void collisionmap_remove_blocked(CollisionMap *map, int tileX, int tileZ) {
    int x = tileX - map->offsetX;
    int z = tileZ - map->offsetZ;
    map->flags[x][z] &= 0xdfffff; // 0xffffff - 0x200000
}

bool collisionmap_test_wall(CollisionMap *map, int sourceX, int sourceZ, int destX, int destZ, int shape, int rotation) {
    if (sourceX == destX && sourceZ == destZ) {
        return true;
    }

    int sx = sourceX - map->offsetX;
    int sz = sourceZ - map->offsetZ;
    int dx = destX - map->offsetX;
    int dz = destZ - map->offsetZ;

    if (shape == WALL_STRAIGHT) {
        if (rotation == 0) {
            if (sx == dx - 1 && sz == dz) {
                return true;
            } else if (sx == dx && sz == dz + 1 && (map->flags[sx][sz] & 0x280120) == 0) {
                return true;
            } else if (sx == dx && sz == dz - 1 && (map->flags[sx][sz] & 0x280102) == 0) {
                return true;
            }
        } else if (rotation == 1) {
            if (sx == dx && sz == dz + 1) {
                return true;
            } else if (sx == dx - 1 && sz == dz && (map->flags[sx][sz] & 0x280108) == 0) {
                return true;
            } else if (sx == dx + 1 && sz == dz && (map->flags[sx][sz] & 0x280180) == 0) {
                return true;
            }
        } else if (rotation == 2) {
            if (sx == dx + 1 && sz == dz) {
                return true;
            } else if (sx == dx && sz == dz + 1 && (map->flags[sx][sz] & 0x280120) == 0) {
                return true;
            } else if (sx == dx && sz == dz - 1 && (map->flags[sx][sz] & 0x280102) == 0) {
                return true;
            }
        } else if (rotation == 3) {
            if (sx == dx && sz == dz - 1) {
                return true;
            } else if (sx == dx - 1 && sz == dz && (map->flags[sx][sz] & 0x280108) == 0) {
                return true;
            } else if (sx == dx + 1 && sz == dz && (map->flags[sx][sz] & 0x280180) == 0) {
                return true;
            }
        }
    } else if (shape == WALL_L) {
        if (rotation == 0) {
            if (sx == dx - 1 && sz == dz) {
                return true;
            } else if (sx == dx && sz == dz + 1) {
                return true;
            } else if (sx == dx + 1 && sz == dz && (map->flags[sx][sz] & 0x280180) == 0) {
                return true;
            } else if (sx == dx && sz == dz - 1 && (map->flags[sx][sz] & 0x280102) == 0) {
                return true;
            }
        } else if (rotation == 1) {
            if (sx == dx - 1 && sz == dz && (map->flags[sx][sz] & 0x280108) == 0) {
                return true;
            } else if (sx == dx && sz == dz + 1) {
                return true;
            } else if (sx == dx + 1 && sz == dz) {
                return true;
            } else if (sx == dx && sz == dz - 1 && (map->flags[sx][sz] & 0x280102) == 0) {
                return true;
            }
        } else if (rotation == 2) {
            if (sx == dx - 1 && sz == dz && (map->flags[sx][sz] & 0x280108) == 0) {
                return true;
            } else if (sx == dx && sz == dz + 1 && (map->flags[sx][sz] & 0x280120) == 0) {
                return true;
            } else if (sx == dx + 1 && sz == dz) {
                return true;
            } else if (sx == dx && sz == dz - 1) {
                return true;
            }
        } else if (rotation == 3) {
            if (sx == dx - 1 && sz == dz) {
                return true;
            } else if (sx == dx && sz == dz + 1 && (map->flags[sx][sz] & 0x280120) == 0) {
                return true;
            } else if (sx == dx + 1 && sz == dz && (map->flags[sx][sz] & 0x280180) == 0) {
                return true;
            } else if (sx == dx && sz == dz - 1) {
                return true;
            }
        }
    } else if (shape == WALL_DIAGONAL) {
        if (sx == dx && sz == dz + 1 && (map->flags[sx][sz] & 0x20) == 0) {
            return true;
        } else if (sx == dx && sz == dz - 1 && (map->flags[sx][sz] & 0x2) == 0) {
            return true;
        } else if (sx == dx - 1 && sz == dz && (map->flags[sx][sz] & 0x8) == 0) {
            return true;
        } else if (sx == dx + 1 && sz == dz && (map->flags[sx][sz] & 0x80) == 0) {
            return true;
        }
    }

    return false;
}

bool collisionmap_test_wdecor(CollisionMap *map, int sourceX, int sourceZ, int destX, int destZ, int shape, int rotation) {
    if (sourceX == destX && sourceZ == destZ) {
        return true;
    }

    int sx = sourceX - map->offsetX;
    int sz = sourceZ - map->offsetZ;
    int dx = destX - map->offsetX;
    int dz = destZ - map->offsetZ;

    if (shape == WALLDECOR_DIAGONAL_NOOFFSET || shape == WALLDECOR_DIAGONAL_OFFSET) {
        if (shape == WALLDECOR_DIAGONAL_OFFSET) {
            rotation = rotation + 2 & 0x3;
        }

        if (rotation == 0) {
            if (sx == dx + 1 && sz == dz && (map->flags[sx][sz] & 0x80) == 0) {
                return true;
            } else if (sx == dx && sz == dz - 1 && (map->flags[sx][sz] & 0x2) == 0) {
                return true;
            }
        } else if (rotation == 1) {
            if (sx == dx - 1 && sz == dz && (map->flags[sx][sz] & 0x8) == 0) {
                return true;
            } else if (sx == dx && sz == dz - 1 && (map->flags[sx][sz] & 0x2) == 0) {
                return true;
            }
        } else if (rotation == 2) {
            if (sx == dx - 1 && sz == dz && (map->flags[sx][sz] & 0x8) == 0) {
                return true;
            } else if (sx == dx && sz == dz + 1 && (map->flags[sx][sz] & 0x20) == 0) {
                return true;
            }
        } else if (rotation == 3) {
            if (sx == dx + 1 && sz == dz && (map->flags[sx][sz] & 0x80) == 0) {
                return true;
            } else if (sx == dx && sz == dz + 1 && (map->flags[sx][sz] & 0x20) == 0) {
                return true;
            }
        }
    } else if (shape == WALLDECOR_DIAGONAL_BOTH) {
        if (sx == dx && sz == dz + 1 && (map->flags[sx][sz] & 0x20) == 0) {
            return true;
        } else if (sx == dx && sz == dz - 1 && (map->flags[sx][sz] & 0x2) == 0) {
            return true;
        } else if (sx == dx - 1 && sz == dz && (map->flags[sx][sz] & 0x8) == 0) {
            return true;
        } else if (sx == dx + 1 && sz == dz && (map->flags[sx][sz] & 0x80) == 0) {
            return true;
        }
    }

    return false;
}

bool collisionmap_test_loc(CollisionMap *map, int srcX, int srcZ, int dstX, int dstZ, int dstSizeX, int dstSizeZ, int forceapproach) {
    int maxX = dstX + dstSizeX - 1;
    int maxZ = dstZ + dstSizeZ - 1;

    if (srcX >= dstX && srcX <= maxX && srcZ >= dstZ && srcZ <= maxZ) {
        return true;
    } else if (srcX == dstX - 1 && srcZ >= dstZ && srcZ <= maxZ && (map->flags[srcX - map->offsetX][srcZ - map->offsetZ] & 0x8) == 0 && (forceapproach & 0x8) == 0) {
        return true;
    } else if (srcX == maxX + 1 && srcZ >= dstZ && srcZ <= maxZ && (map->flags[srcX - map->offsetX][srcZ - map->offsetZ] & 0x80) == 0 && (forceapproach & 0x2) == 0) {
        return true;
    } else if (srcZ == dstZ - 1 && srcX >= dstX && srcX <= maxX && (map->flags[srcX - map->offsetX][srcZ - map->offsetZ] & 0x2) == 0 && (forceapproach & 0x4) == 0) {
        return true;
    } else if (srcZ == maxZ + 1 && srcX >= dstX && srcX <= maxX && (map->flags[srcX - map->offsetX][srcZ - map->offsetZ] & 0x20) == 0 && (forceapproach & 0x1) == 0) {
        return true;
    }

    return false;
}
