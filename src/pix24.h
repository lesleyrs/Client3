#pragma once

#include "jagfile.h"
#include "pix8.h"

typedef struct {
    DoublyLinkable link;
    int *pixels;
    int width;
    int height;
    int crop_x;
    int crop_y;
    int crop_w;
    int crop_h;
} Pix24;

Pix24 *pix24_new(int width, int height, bool use_allocator);
void pix24_free(Pix24 *pix24);
Pix24 *pix24_from_jpeg(uint8_t* data, int length);
Pix24 *pix24_from_archive(Jagfile *jag, const char *name, int index);
void pix24_bind(Pix24 *pix24);
void pix24_translate(Pix24 *pix24, int r, int g, int b);
void pix24_blit_opaque(Pix24 *pix24, int x, int y);
void pix24_copy_pixels(int w, int h, int *src, int srcOff, int srcStep, int *dst, int dstOff, int dstStep);
void pix24_draw(Pix24 *pix24, int x, int y);
void pix24_copy_pixels2(int *dst, int *src, int srcOff, int dstOff, int w, int h, int dstStep, int srcStep);
void pix24_crop(Pix24 *pix24, int x, int y, int w, int h);
void pix24_scale(int w, int h, int *src, int offW, int offH, int *dst, int dstStep, int dstOff, int currentW, int scaleCropWidth, int scaleCropHeight);
void pix24_draw_alpha(Pix24 *pix24, int alpha, int x, int y);
void pix24_copy_pixels_alpha(int w, int h, int *src, int srcOff, int srcStep, int *dst, int dstOff, int dstStep, int alpha);
void pix24_draw_rotated_masked(Pix24 *pix24, int x, int y, int w, int h, int *lineStart, int *lineWidth, int anchorX, int anchorY, int theta, int zoom);
void pix24_draw_masked(Pix24 *pix24, int x, int y, Pix8 *mask);
void pix24_copy_pixels_masked(int w, int h, int *src, int srcStep, int srcOff, int *dst, int dstOff, int dstStep, int8_t *mask);
