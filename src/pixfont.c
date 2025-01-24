#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "jagfile.h"
#include "packet.h"
#include "pix2d.h"
#include "pixfont.h"
#include "platform.h"

const char CHARSET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!\"A$%^&*()-_=+[{]};:'@#~,<.>/?\\| ";
int CHARCODESET[256] = {0};

void pixfont_init_global(void) {
    // NOTE disabled pound, doesn't fit in C char
    const char *charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!\"A$%^&*()-_=+[{]};:'@#~,<.>/?\\| ";
    // const char* charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!\"£$%^&*()-_=+[{]};:'@#~,<.>/?\\| ";

    for (int i = 0; i < 256; i++) {
        // TODO check if same result as java
        int c = indexof(charset, (const char[]){i, '\0'});
        if (c == -1) {
            c = 74;
        }

        CHARCODESET[i] = c;
    }
}

extern Pix2D _Pix2D;

PixFont *pixfont_new(void) {
    PixFont *pixfont = calloc(1, sizeof(PixFont));
    pixfont->charMask = malloc(94 * sizeof(int8_t *));
    pixfont->charMaskWidth = malloc(94 * sizeof(int));
    pixfont->charMaskHeight = malloc(94 * sizeof(int));
    pixfont->charOffsetX = malloc(94 * sizeof(int));
    pixfont->charOffsetY = malloc(94 * sizeof(int));
    pixfont->charAdvance = malloc(95 * sizeof(int));
    pixfont->drawWidth = malloc(256 * sizeof(int));
    pixfont->height = false;
    // pixfont->random;// = new Random(); // NOTE does this matter at all
    return pixfont;
}

void pixfont_free(PixFont *pixfont) {
    for (int i = 0; i < 94; i++) {
        free(pixfont->charMask[i]);
    }
    free(pixfont->charMask);
    free(pixfont->charMaskWidth);
    free(pixfont->charMaskHeight);
    free(pixfont->charOffsetX);
    free(pixfont->charOffsetY);
    free(pixfont->charAdvance);
    free(pixfont->drawWidth);
    // pixfont->random;// = new Random();
    free(pixfont);
}

PixFont *pixfont_from_archive(Jagfile *title, const char *font) {
    size_t name_length = strlen(font) + 5;
    char *filename = malloc(name_length);
    snprintf(filename, name_length, "%s.dat", font);
    Packet *dat = jagfile_to_packet(title, filename);
    Packet *idx = jagfile_to_packet(title, "index.dat");
    idx->pos = g2(dat) + 4;

    int off = g1(idx);
    if (off > 0) {
        idx->pos += (off - 1) * 3;
    }

    PixFont *pixfont = pixfont_new();
    for (int i = 0; i < 94; i++) {
        pixfont->charOffsetX[i] = g1(idx);
        pixfont->charOffsetY[i] = g1(idx);

        int w = pixfont->charMaskWidth[i] = g2(idx);
        int h = pixfont->charMaskHeight[i] = g2(idx);

        int type = g1(idx);
        int len = w * h;

        pixfont->charMask[i] = malloc(len);

        if (type == 0) {
            for (int j = 0; j < len; j++) {
                pixfont->charMask[i][j] = g1b(dat);
            }
        } else if (type == 1) {
            for (int x = 0; x < w; x++) {
                for (int y = 0; y < h; y++) {
                    pixfont->charMask[i][x + y * w] = g1b(dat);
                }
            }
        }

        if (h > pixfont->height) {
            pixfont->height = h;
        }

        pixfont->charOffsetX[i] = 1;
        pixfont->charAdvance[i] = w + 2;

        int space = 0;
        for (int j = h / 7; j < h; j++) {
            space += pixfont->charMask[i][j * w];
        }

        if (space <= h / 7) {
            pixfont->charAdvance[i]--;
            pixfont->charOffsetX[i] = 0;
        }

        space = 0;
        for (int j = h / 7; j < h; j++) {
            space += pixfont->charMask[i][w + j * w - 1];
        }

        if (space <= h / 7) {
            pixfont->charAdvance[i]--;
        }
    }

    pixfont->charAdvance[94] = pixfont->charAdvance[8];
    for (int c = 0; c < 256; c++) {
        pixfont->drawWidth[c] = pixfont->charAdvance[CHARCODESET[c]];
    }
    free(filename);
    packet_free(dat);
    packet_free(idx);
    return pixfont;
}

void drawStringCenter(PixFont *pixfont, int x, int y, const char *str, int rgb) {
    drawString(pixfont, x - stringWidth(pixfont, str) / 2, y, str, rgb);
}

void drawStringTaggableCenter(PixFont *pixfont, const char *str, int x, int y, int color, bool shadowed) {
    drawStringTaggable(pixfont, x - stringWidth(pixfont, str) / 2, y, str, color, shadowed);
}

int stringWidth(PixFont *pixfont, const char *str) {
    if (!str) {
        return 0;
    }

    int size = 0;
    for (size_t c = 0; c < strlen(str); c++) {
        if (str[c] == '@' && c + 4 < strlen(str) && str[c + 4] == '@') {
            c += 4;
        } else {
            size += pixfont->drawWidth[(unsigned char)str[c]];
        }
    }

    return size;
}

void drawString(PixFont *pixfont, int x, int y, const char *str, int rgb) {
    if (!str) {
        return;
    }

    int offY = y - pixfont->height;

    for (size_t i = 0; i < strlen(str); i++) {
        int c = CHARCODESET[(unsigned char)str[i]];
        if (c != 94) {
            drawChar(pixfont->charMask[c], x + pixfont->charOffsetX[c], offY + pixfont->charOffsetY[c], pixfont->charMaskWidth[c], pixfont->charMaskHeight[c], rgb);
        }

        x += pixfont->charAdvance[c];
    }
}

void drawStringRight(PixFont *pixfont, int x, int y, const char *str, int rgb, bool shadowed) {
    if (shadowed) {
        drawString(pixfont, x + 1 - stringWidth(pixfont, str), y + 1, str, BLACK);
    }

    drawString(pixfont, x - stringWidth(pixfont, str), y, str, rgb);
}

void drawCenteredWave(PixFont *pixfont, int x, int y, const char *str, int rgb, int phase) {
    if (!str) {
        return;
    }

    x -= stringWidth(pixfont, str) / 2;
    int offY = y - pixfont->height;

    for (size_t i = 0; i < strlen(str); i++) {
        int c = CHARCODESET[(unsigned char)str[i]];

        if (c != 94) {
            drawChar(pixfont->charMask[c], x + pixfont->charOffsetX[c], offY + pixfont->charOffsetY[c] + (int)(sin((double)i / 2.0 + (double)phase / 5.0) * 5.0), pixfont->charMaskWidth[c], pixfont->charMaskHeight[c], rgb);
        }

        x += pixfont->charAdvance[c];
    }
}

void drawStringTaggable(PixFont *pixfont, int x, int y, const char *str, int rgb, bool shadowed) {
    if (!str) {
        return;
    }

    int offY = y - pixfont->height;

    for (size_t i = 0; i < strlen(str); i++) {
        if (str[i] == '@' && i + 4 < strlen(str) && str[i + 4] == '@') {
            char *sub = substring(str, i + 1, i + 4);
            rgb = evaluateTag(sub);
            free(sub);
            i += 4;
        } else {
            int c = CHARCODESET[(unsigned char)str[i]];
            if (c != 94) {
                if (shadowed) {
                    drawChar(pixfont->charMask[c], x + pixfont->charOffsetX[c] + 1, offY + pixfont->charOffsetY[c] + 1, pixfont->charMaskWidth[c], pixfont->charMaskHeight[c], 0);
                }

                drawChar(pixfont->charMask[c], x + pixfont->charOffsetX[c], offY + pixfont->charOffsetY[c], pixfont->charMaskWidth[c], pixfont->charMaskHeight[c], rgb);
            }

            x += pixfont->charAdvance[c];
        }
    }
}

void drawStringTooltip(PixFont *pixfont, int x, int y, const char *str, int color, bool shadowed, int seed) {
    if (!str) {
        return;
    }

    // TODO: temp, it's fine I guess?
    srand(seed);

    int random = (rand() & 0x1f) + 192;
    int offY = y - pixfont->height;
    for (size_t i = 0; i < strlen(str); i++) {
        if (str[i] == '@' && i + 4 < strlen(str) && str[i + 4] == '@') {
            char *sub = substring(str, i + 1, i + 4);
            color = evaluateTag(sub);
            free(sub);
            i += 4;
        } else {
            int c = CHARCODESET[(unsigned char)str[i]];
            if (c != 94) {
                if (shadowed) {
                    drawCharAlpha(x + pixfont->charOffsetX[c] + 1, offY + pixfont->charOffsetY[c] + 1, pixfont->charMaskWidth[c], pixfont->charMaskHeight[c], 0, 192, pixfont->charMask[c]);
                }

                drawCharAlpha(x + pixfont->charOffsetX[c], offY + pixfont->charOffsetY[c], pixfont->charMaskWidth[c], pixfont->charMaskHeight[c], color, random, pixfont->charMask[c]);
            }

            x += pixfont->charAdvance[c];
            if ((rand() & 0x3) == 0) {
                x++;
            }
        }
    }
}

int evaluateTag(const char *tag) {
    if (strcmp(tag, "red") == 0) {
        return RED;
    } else if (strcmp(tag, "gre") == 0) {
        return GREEN;
    } else if (strcmp(tag, "blu") == 0) {
        return BLUE;
    } else if (strcmp(tag, "yel") == 0) {
        return YELLOW;
    } else if (strcmp(tag, "cya") == 0) {
        return CYAN;
    } else if (strcmp(tag, "mag") == 0) {
        return MAGENTA;
    } else if (strcmp(tag, "whi") == 0) {
        return WHITE;
    } else if (strcmp(tag, "bla") == 0) {
        return BLACK;
    } else if (strcmp(tag, "lre") == 0) {
        return LIGHTRED;
    } else if (strcmp(tag, "dre") == 0) {
        return DARKRED;
    } else if (strcmp(tag, "dbl") == 0) {
        return DARKBLUE;
    } else if (strcmp(tag, "or1") == 0) {
        return ORANGE1;
    } else if (strcmp(tag, "or2") == 0) {
        return ORANGE2;
    } else if (strcmp(tag, "or3") == 0) {
        return ORANGE3;
    } else if (strcmp(tag, "gr1") == 0) {
        return GREEN1;
    } else if (strcmp(tag, "gr2") == 0) {
        return GREEN2;
    } else if (strcmp(tag, "gr3") == 0) {
        return GREEN3;
    } else {
        return BLACK;
    }
}

void drawChar(int8_t *data, int x, int y, int w, int h, int rgb) {
    int dstOff = x + y * _Pix2D.width;
    int dstStep = _Pix2D.width - w;

    int srcStep = 0;
    int srcOff = 0;

    if (y < _Pix2D.top) {
        int cutoff = _Pix2D.top - y;
        h -= cutoff;
        y = _Pix2D.top;
        srcOff += cutoff * w;
        dstOff += cutoff * _Pix2D.width;
    }

    if (y + h >= _Pix2D.bottom) {
        h -= y + h + 1 - _Pix2D.bottom;
    }

    if (x < _Pix2D.left) {
        int cutoff = _Pix2D.left - x;
        w -= cutoff;
        x = _Pix2D.left;
        srcOff += cutoff;
        dstOff += cutoff;
        srcStep += cutoff;
        dstStep += cutoff;
    }

    if (x + w >= _Pix2D.right) {
        int cutoff = x + w + 1 - _Pix2D.right;
        w -= cutoff;
        srcStep += cutoff;
        dstStep += cutoff;
    }

    if (w > 0 && h > 0) {
        drawMask(w, h, data, srcOff, srcStep, _Pix2D.pixels, dstOff, dstStep, rgb);
    }
}

void drawMask(int w, int h, int8_t *src, int srcOff, int srcStep, int *dst, int dstOff, int dstStep, int rgb) {
    int hw = -(w >> 2);
    w = -(w & 0x3);

    for (int y = -h; y < 0; y++) {
        for (int x = hw; x < 0; x++) {
            if (src[srcOff++] == 0) {
                dstOff++;
            } else {
                dst[dstOff++] = rgb;
            }

            if (src[srcOff++] == 0) {
                dstOff++;
            } else {
                dst[dstOff++] = rgb;
            }

            if (src[srcOff++] == 0) {
                dstOff++;
            } else {
                dst[dstOff++] = rgb;
            }

            if (src[srcOff++] == 0) {
                dstOff++;
            } else {
                dst[dstOff++] = rgb;
            }
        }

        for (int x = w; x < 0; x++) {
            if (src[srcOff++] == 0) {
                dstOff++;
            } else {
                dst[dstOff++] = rgb;
            }
        }

        dstOff += dstStep;
        srcOff += srcStep;
    }
}

void drawCharAlpha(int x, int y, int w, int h, int rgb, int alpha, int8_t *mask) {
    int dstOff = x + y * _Pix2D.width;
    int dstStep = _Pix2D.width - w;

    int srcStep = 0;
    int srcOff = 0;

    if (y < _Pix2D.top) {
        int cutoff = _Pix2D.top - y;
        h -= cutoff;
        y = _Pix2D.top;
        srcOff += cutoff * w;
        dstOff += cutoff * _Pix2D.width;
    }

    if (y + h >= _Pix2D.bottom) {
        h -= y + h + 1 - _Pix2D.bottom;
    }

    if (x < _Pix2D.left) {
        int cutoff = _Pix2D.left - x;
        w -= cutoff;
        x = _Pix2D.left;
        srcOff += cutoff;
        dstOff += cutoff;
        srcStep += cutoff;
        dstStep += cutoff;
    }

    if (x + w >= _Pix2D.right) {
        int cutoff = x + w + 1 - _Pix2D.right;
        w -= cutoff;
        srcStep += cutoff;
        dstStep += cutoff;
    }

    if (w > 0 && h > 0) {
        drawMaskAlpha(w, h, _Pix2D.pixels, dstOff, dstStep, mask, srcOff, srcStep, rgb, alpha);
    }
}

void drawMaskAlpha(int w, int h, int *dst, int dstOff, int dstStep, int8_t *mask, int maskOff, int maskStep, int color, int alpha) {
    int rgb = (((color & 0xff00ff) * alpha & 0xff00ff00) + ((color & 0xff00) * alpha & 0xff0000)) >> 8;
    int invAlpha = 256 - alpha;

    for (int y = -h; y < 0; y++) {
        for (int x = -w; x < 0; x++) {
            if (mask[maskOff++] == 0) {
                dstOff++;
            } else {
                int dstRgb = dst[dstOff];
                dst[dstOff++] = ((((dstRgb & 0xff00ff) * invAlpha & 0xff00ff00) + ((dstRgb & 0xff00) * invAlpha & 0xff0000)) >> 8) + rgb;
            }
        }

        dstOff += dstStep;
        maskOff += maskStep;
    }
}
