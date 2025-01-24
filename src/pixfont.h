#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "datastruct/doublylinkable.h"
#include "jagfile.h"

extern const char CHARSET[];

typedef struct {
    DoublyLinkable link;
    int8_t **charMask;
    int *charMaskWidth;
    int *charMaskHeight;
    int *charOffsetX;
    int *charOffsetY;
    int *charAdvance;
    int *drawWidth;
    int height;
} PixFont;

PixFont *pixfont_new(void);
void pixfont_free(PixFont *pixfont);
void pixfont_init_global(void);
PixFont *pixfont_from_archive(Jagfile *title, const char *font);
void drawStringCenter(PixFont *pixfont, int x, int y, const char *str, int rgb);
void drawStringTaggableCenter(PixFont *pixfont, const char *str, int x, int y, int color, bool shadowed);
int stringWidth(PixFont *pixfont, const char *str);
void drawString(PixFont *pixfont, int x, int y, const char *str, int rgb);
void drawStringRight(PixFont *pixfont, int x, int y, const char *str, int rgb, bool shadowed);
void drawCenteredWave(PixFont *pixfont, int x, int y, const char *str, int rgb, int phase);
void drawStringTaggable(PixFont *pixfont, int x, int y, const char *str, int rgb, bool shadowed);
void drawStringTooltip(PixFont *pixfont, int x, int y, const char *str, int color, bool shadowed, int seed);
int evaluateTag(const char *tag);
void drawChar(int8_t *data, int x, int y, int w, int h, int rgb);
void drawMask(int w, int h, int8_t *src, int srcOff, int srcStep, int *dst, int dstOff, int dstStep, int rgb);
void drawCharAlpha(int x, int y, int w, int h, int rgb, int alpha, int8_t *mask);
void drawMaskAlpha(int w, int h, int *dst, int dstOff, int dstStep, int8_t *mask, int maskOff, int maskStep, int color, int alpha);
