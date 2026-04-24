#pragma once

#include <stdint.h>

#include "datastruct/doublylinkable.h"
#include "jagfile.h"

typedef struct {
    DoublyLinkable link;
    int8_t *pixels;
    int width;
    int height;
    int crop_x;
    int crop_y;
    int crop_w;
    int crop_h;
    int *palette;
    int palette_count;
} Pix8;

Pix8 *pix8_new(int width, int height, int *palette, int palette_count);
void pix8_free(Pix8 *pix8);
Pix8 *pix8_from_archive(Jagfile *jag, const char *name, int sprite);
void pix8_shrink(Pix8 *pix8);
void pix8_crop(Pix8 *pix8);
void pix8_flip_horizontally(Pix8 *pix8);
void pix8_flip_vertically(Pix8 *pix8);
void pix8_translate(Pix8 *pix8, int r, int g, int b);
void pix8_draw(Pix8 *pix8, int x, int y);
void pix8_copy_pixels(int w, int h, int8_t *src, int srcOff, int srcStep, int *dst, int dstOff, int dstStep, int *palette);
