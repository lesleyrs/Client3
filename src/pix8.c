#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jagfile.h"
#include "packet.h"
#include "pix2d.h"
#include "pix8.h"

extern Pix2D _Pix2D;

Pix8 *pix8_new(int width, int height, int *palette, int palette_count) {
    Pix8 *pix8 = calloc(1, sizeof(Pix8));
    pix8->width = pix8->crop_w = width;
    pix8->height = pix8->crop_h = height;
    pix8->crop_x = pix8->crop_y = 0;
    pix8->palette = palette;
    pix8->palette_count = palette_count;
    return pix8;
}

void pix8_free(Pix8 *pix8) {
    free(pix8->pixels);
    free(pix8->palette);
    free(pix8);
}

Pix8 *pix8_from_archive(Jagfile *jag, const char *name, int sprite) {
    size_t name_length = strlen(name) + 5;
    char *filename = calloc(name_length, sizeof(char));
    snprintf(filename, name_length, "%s.dat", name);

    Packet *dat = jagfile_to_packet(jag, filename);
    Packet *idx = jagfile_to_packet(jag, "index.dat");
    idx->pos = g2(dat);
    const int crop_w = g2(idx);
    const int crop_h = g2(idx);
    int palette_count = g1(idx);
    int *palette = malloc(palette_count * sizeof(int));
    palette[0] = 0;
    for (int i = 0; i < palette_count - 1; i++) {
        palette[i + 1] = g3(idx);
    }
    Pix8 *pix8 = pix8_new(crop_w, crop_h, palette, palette_count);
    for (int i = 0; i < sprite; i++) {
        idx->pos += 2;
        dat->pos += g2(idx) * g2(idx);
        idx->pos++;
    }
    pix8->crop_x = g1(idx);
    pix8->crop_y = g1(idx);
    pix8->width = g2(idx);
    pix8->height = g2(idx);
    int pixel_order = g1(idx);
    int len = pix8->width * pix8->height;
    pix8->pixels = calloc(len, sizeof(int8_t));
    if (pixel_order == 0) {
        for (int i = 0; i < len; i++) {
            pix8->pixels[i] = g1b(dat);
        }
    } else if (pixel_order == 1) {
        for (int x = 0; x < pix8->width; x++) {
            for (int y = 0; y < pix8->height; y++) {
                pix8->pixels[x + y * pix8->width] = g1b(dat);
            }
        }
    }
    free(filename);
    packet_free(dat);
    packet_free(idx);
    return pix8;
}

void pix8_shrink(Pix8 *pix8) {
    pix8->crop_w /= 2;
    pix8->crop_h /= 2;

    int8_t *pixels = calloc(pix8->crop_w * pix8->crop_h, sizeof(int8_t));
    int off = 0;
    for (int y = 0; y < pix8->height; y++) {
        for (int x = 0; x < pix8->width; x++) {
            pixels[((x + pix8->crop_x) >> 1) + ((y + pix8->crop_y) >> 1) * pix8->crop_w] = pix8->pixels[off++];
        }
    }

    free(pix8->pixels);
    pix8->pixels = pixels;
    pix8->width = pix8->crop_w;
    pix8->height = pix8->crop_h;
    pix8->crop_x = 0;
    pix8->crop_y = 0;
}

void pix8_crop(Pix8 *pix8) {
    if (pix8->width == pix8->crop_w && pix8->height == pix8->crop_h) {
        return;
    }

    int8_t *pixels = calloc(pix8->crop_w * pix8->crop_h, sizeof(int8_t));
    int off = 0;
    for (int y = 0; y < pix8->height; y++) {
        for (int x = 0; x < pix8->width; x++) {
            pixels[x + pix8->crop_x + (y + pix8->crop_y) * pix8->crop_w] = pix8->pixels[off++];
        }
    }

    free(pix8->pixels);
    pix8->pixels = pixels;
    pix8->width = pix8->crop_w;
    pix8->height = pix8->crop_h;
    pix8->crop_x = 0;
    pix8->crop_y = 0;
}

void pix8_flip_horizontally(Pix8 *pix8) {
    int8_t *pixels = calloc(pix8->width * pix8->height, sizeof(int8_t));
    int off = 0;
    for (int y = 0; y < pix8->height; y++) {
        for (int x = pix8->width - 1; x >= 0; x--) {
            pixels[off++] = pix8->pixels[x + y * pix8->width];
        }
    }

    free(pix8->pixels);
    pix8->pixels = pixels;
    pix8->crop_x = pix8->crop_w - pix8->width - pix8->crop_x;
}

void pix8_flip_vertically(Pix8 *pix8) {
    int8_t *pixels = calloc(pix8->width * pix8->height, sizeof(int8_t));
    int off = 0;
    for (int y = pix8->height - 1; y >= 0; y--) {
        for (int x = 0; x < pix8->width; x++) {
            pixels[off++] = pix8->pixels[x + y * pix8->width];
        }
    }

    free(pix8->pixels);
    pix8->pixels = pixels;
    pix8->crop_y = pix8->crop_h - pix8->height - pix8->crop_y;
}

void pix8_translate(Pix8 *pix8, int r, int g, int b) {
    for (int i = 0; i < pix8->palette_count; i++) {
        int red = pix8->palette[i] >> 16 & 0xff;
        red += r;
        if (red < 0) {
            red = 0;
        } else if (red > 255) {
            red = 255;
        }

        int green = pix8->palette[i] >> 8 & 0xff;
        green += g;
        if (green < 0) {
            green = 0;
        } else if (green > 255) {
            green = 255;
        }

        int blue = pix8->palette[i] & 0xff;
        blue += b;
        if (blue < 0) {
            blue = 0;
        } else if (blue > 255) {
            blue = 255;
        }

        pix8->palette[i] = (red << 16) + (green << 8) + blue;
    }
}

void pix8_draw(Pix8 *pix8, int x, int y) {
    x += pix8->crop_x;
    y += pix8->crop_y;

    int dstOff = x + y * _Pix2D.width;
    int srcOff = 0;
    int h = pix8->height;
    int w = pix8->width;
    int dstStep = _Pix2D.width - w;
    int srcStep = 0;

    if (y < _Pix2D.top) {
        int cutoff = _Pix2D.top - y;
        h -= cutoff;
        y = _Pix2D.top;
        srcOff += cutoff * w;
        dstOff += cutoff * _Pix2D.width;
    }

    if (y + h > _Pix2D.bottom) {
        h -= y + h - _Pix2D.bottom;
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

    if (x + w > _Pix2D.right) {
        int cutoff = x + w - _Pix2D.right;
        w -= cutoff;
        srcStep += cutoff;
        dstStep += cutoff;
    }

    if (w > 0 && h > 0) {
        pix8_copy_pixels(w, h, pix8->pixels, srcOff, srcStep, _Pix2D.pixels, dstOff, dstStep, pix8->palette);
    }
}

void pix8_copy_pixels(int w, int h, int8_t *src, int srcOff, int srcStep, int *dst, int dstOff, int dstStep, int *palette) {
    int qw = -(w >> 2);
    w = -(w & 0x3);

    for (int y = -h; y < 0; y++) {
        for (int x = qw; x < 0; x++) {
            int8_t palIndex = src[srcOff++];
            if (palIndex == 0) {
                dstOff++;
            } else {
                dst[dstOff++] = palette[palIndex & 0xff];
            }

            palIndex = src[srcOff++];
            if (palIndex == 0) {
                dstOff++;
            } else {
                dst[dstOff++] = palette[palIndex & 0xff];
            }

            palIndex = src[srcOff++];
            if (palIndex == 0) {
                dstOff++;
            } else {
                dst[dstOff++] = palette[palIndex & 0xff];
            }

            palIndex = src[srcOff++];
            if (palIndex == 0) {
                dstOff++;
            } else {
                dst[dstOff++] = palette[palIndex & 0xff];
            }
        }

        for (int x = w; x < 0; x++) {
            int8_t palIndex = src[srcOff++];
            if (palIndex == 0) {
                dstOff++;
            } else {
                dst[dstOff++] = palette[palIndex & 0xff];
            }
        }

        dstOff += dstStep;
        srcOff += srcStep;
    }
}
