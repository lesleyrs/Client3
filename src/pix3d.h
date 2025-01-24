#pragma once

#include <stdbool.h>

#include "pix2d.h"
#include "pix8.h"

typedef struct {
    DoublyLinkable link;
    Pix2D pix2d;
    bool clipX;
    bool opaque;
    int alpha;
    int center_x;
    int center_y;
    int *line_offset;
    int textureCount;
    int poolSize;
    int **texelPool;
    int cycle;
    bool lowMemory;
    bool jagged;
    int *reciprical15;
    int *reciprical16;
    int *sin_table;
    int *cos_table;
    Pix8 **textures;
    bool *textureHasTransparency;
    int *textureColors;
    int **activeTexels;
    int *textureCycle;
    int *palette;
    int **texturePalettes;

    int renderWidth;
    int renderHeight;
} Pix3D;

void pix3d_init_global(void);
void pix3d_free_global(void);
void pix3d_init2d(void);
void pix3d_init3d(int width, int height);
void pix3d_clear_texels(void);
void pix3d_init_pool(int size);
void pix3d_unpack_textures(Jagfile *jag);
int pix3d_get_average_texture_rgb(int id);
void pix3d_push_texture(int id);
void pix3d_set_brightness(double brightness);
int pix3d_set_gamma(int rgb, double gamma);

void gouraudTriangle(int xA, int xB, int xC, int yA, int yB, int yC, int colorA, int colorB, int colorC);
void flatTriangle(int xA, int xB, int xC, int yA, int yB, int yC, int color);
void textureTriangle(int xA, int xB, int xC, int yA, int yB, int yC, int shadeA, int shadeB, int shadeC, int originX, int originY, int originZ, int txB, int txC, int tyB, int tyC, int tzB, int tzC, int texture);
