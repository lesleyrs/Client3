#pragma once

#include "datastruct/doublylinkable.h"

typedef struct {
    DoublyLinkable link;
    int *pixels;
    int width;
    int height;
    int top;
    int bottom;
    int left;
    int right;
    int bound_x;
    int center_x;
    int center_y;
} Pix2D;

// TODO: ts Client2/java Client (lastmain/webclient branch) repo have extra funcs

void pix2d_bind(int width, int height, int *pixels);
void pix2d_reset_clipping(void);
void pix2d_set_clipping(int bottom, int right, int top, int left);
void pix2d_clear(void);
void pix2d_fill_rect(int x, int y, int rgb, int w, int h);
void pix2d_draw_rect(int x, int y, int rgb, int w, int h);
void pix2d_hline(int x, int y, int rgb, int w);
void pix2d_vline(int x, int y, int rgb, int h);
