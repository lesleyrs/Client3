#pragma once

#include "platform.h"

struct PixMap {
    int width;
    int height;
    Surface *image;
    int *pixels;
};

PixMap *pixmap_new(int width, int height);
void pixmap_free(PixMap *pixmap);
void pixmap_clear(PixMap *pixmap);
void pixmap_bind(PixMap *pixmap);
void pixmap_draw(PixMap *pixmap, int x, int y);
