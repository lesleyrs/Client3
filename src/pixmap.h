#pragma once

#include "platform.h"

struct PixMap {
    int width;
    int height;
    int *pixels;
#ifndef GL11
    Surface *image;
#else
    float u;
    float v;
    int *gl_pixels;
    uint32_t gl_texture;
    bool dirty;
#endif
};

PixMap *pixmap_new(int width, int height);
void pixmap_free(PixMap *pixmap);
void pixmap_bind(PixMap *pixmap);
void pixmap_draw(PixMap *pixmap, int x, int y);
