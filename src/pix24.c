#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "allocator.h"
#include "jagfile.h"
#include "packet.h"
#include "pix24.h"
#include "pix2d.h"
#include "pix8.h"
#include "platform.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_THREAD_LOCALS
#define STBI_ONLY_JPEG
#define STBI_NO_HDR
#define STBI_NO_SIMD
#define STBI_NO_FAILURE_STRINGS
#ifdef NXDK
#undef _MSC_VER
#endif
#include "thirdparty/stb_image.h"

extern Pix2D _Pix2D;

Pix24 *pix24_new(int width, int height, bool use_allocator) {
    Pix24 *pix24 = rs2_calloc(use_allocator, 1, sizeof(Pix24));
    pix24->pixels = rs2_calloc(use_allocator, width * height, sizeof(int));
    pix24->width = pix24->crop_w = width;
    pix24->height = pix24->crop_h = height;
    pix24->crop_x = pix24->crop_y = 0;
    return pix24;
}

void pix24_free(Pix24 *pix24) {
    free(pix24->pixels);
    free(pix24);
}

Pix24 *pix24_from_jpeg(uint8_t* data, int length) {
    int x, y, n;
    uint8_t *jpeg_data = stbi_load_from_memory(data, length, &x, &y, &n, 0);
    if (!jpeg_data) {
        rs2_error("Error converting jpg");
        return NULL;
    }

    Pix24 *pix24 = pix24_new(x, y, false);
    for (int i = 0; i < x * y; i++) {
        pix24->pixels[i] = (jpeg_data[i * n] << 16) | (jpeg_data[i * n + 1] << 8) | jpeg_data[i * n + 2];
    }
    stbi_image_free(jpeg_data);
    return pix24;
}

Pix24 *pix24_from_archive(Jagfile *jag, const char *name, int index) {
    size_t name_length = strlen(name) + 5;
    char *filename = calloc(name_length, sizeof(char));
    snprintf(filename, name_length, "%s.dat", name);

    Packet *dat = jagfile_to_packet(jag, filename);
    Packet *idx = jagfile_to_packet(jag, "index.dat");

    idx->pos = g2(dat);
    Pix24 *pix24 = calloc(1, sizeof(Pix24));
    pix24->crop_w = g2(idx);
    pix24->crop_h = g2(idx);

    int paletteCount = g1(idx);
    int *palette = malloc(paletteCount * sizeof(int));
    palette[0] = 0;
    for (int i = 0; i < paletteCount - 1; i++) {
        palette[i + 1] = g3(idx);
        if (palette[i + 1] == 0) {
            palette[i + 1] = 1;
        }
    }

    for (int i = 0; i < index; i++) {
        idx->pos += 2;
        dat->pos += g2(idx) * g2(idx);
        idx->pos++;
    }

    if (dat->pos >= dat->length || idx->pos >= idx->length) {
        // rs2_error("pix24 id %d %s %i >= %i || %i >= %i\n", index, name, dat->pos, dat->length, idx->pos, idx->length);
        free(palette);
        free(filename);
        packet_free(dat);
        packet_free(idx);
        free(pix24);
        return NULL;
    }

    pix24->crop_x = g1(idx);
    pix24->crop_y = g1(idx);
    pix24->width = g2(idx);
    pix24->height = g2(idx);
    pix24->pixels = calloc(pix24->width * pix24->height, sizeof(int));

    const int pixelOrder = g1(idx);

    if (pixelOrder == 0) {
        for (int i = 0; i < pix24->width * pix24->height; i++) {
            pix24->pixels[i] = palette[g1(dat)];
        }
    } else if (pixelOrder == 1) {
        for (int x = 0; x < pix24->width; x++) {
            for (int y = 0; y < pix24->height; y++) {
                pix24->pixels[x + y * pix24->width] = palette[g1(dat)];
            }
        }
    }
    free(palette);
    free(filename);
    packet_free(dat);
    packet_free(idx);
    return pix24;
}

void pix24_bind(Pix24 *pix24) {
    pix2d_bind(pix24->width, pix24->height, pix24->pixels);
}

void pix24_translate(Pix24 *pix24, int r, int g, int b) {
    for (int i = 0; i < pix24->width * pix24->height; i++) {
        int rgb = pix24->pixels[i];

        if (rgb != 0) {
            int red = rgb >> 16 & 0xff;
            red += r;
            if (red < 1) {
                red = 1;
            } else if (red > 255) {
                red = 255;
            }

            int green = rgb >> 8 & 0xff;
            green += g;
            if (green < 1) {
                green = 1;
            } else if (green > 255) {
                green = 255;
            }

            int blue = rgb & 0xff;
            blue += b;
            if (blue < 1) {
                blue = 1;
            } else if (blue > 255) {
                blue = 255;
            }

            pix24->pixels[i] = (red << 16) + (green << 8) + blue;
        }
    }
}

void pix24_blit_opaque(Pix24 *pix24, int x, int y) {
    x += pix24->crop_x;
    y += pix24->crop_y;

    int dstOff = x + y * _Pix2D.width;
    int srcOff = 0;
    int h = pix24->height;
    int w = pix24->width;
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
        pix24_copy_pixels(w, h, pix24->pixels, srcOff, srcStep, _Pix2D.pixels, dstOff, dstStep);
    }
}

void pix24_copy_pixels(int w, int h, int *src, int srcOff, int srcStep, int *dst, int dstOff, int dstStep) {
    int qw = -(w >> 2);
    w = -(w & 0x3);

    for (int y = -h; y < 0; y++) {
        for (int x = qw; x < 0; x++) {
            dst[dstOff++] = src[srcOff++];
            dst[dstOff++] = src[srcOff++];
            dst[dstOff++] = src[srcOff++];
            dst[dstOff++] = src[srcOff++];
        }

        for (int x = w; x < 0; x++) {
            dst[dstOff++] = src[srcOff++];
        }

        dstOff += dstStep;
        srcOff += srcStep;
    }
}

void pix24_draw(Pix24 *pix24, int x, int y) {
    x += pix24->crop_x;
    y += pix24->crop_y;

    int dstOff = x + y * _Pix2D.width;
    int srcOff = 0;
    int h = pix24->height;
    int w = pix24->width;
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
        pix24_copy_pixels2(_Pix2D.pixels, pix24->pixels, srcOff, dstOff, w, h, dstStep, srcStep);
    }
}

void pix24_copy_pixels2(int *dst, int *src, int srcOff, int dstOff, int w, int h, int dstStep, int srcStep) {
    int qw = -(w >> 2);
    w = -(w & 0x3);

    for (int y = -h; y < 0; y++) {
        for (int x = qw; x < 0; x++) {
            int rgb = src[srcOff++];
            if (rgb == 0) {
                dstOff++;
            } else {
                dst[dstOff++] = rgb;
            }

            rgb = src[srcOff++];
            if (rgb == 0) {
                dstOff++;
            } else {
                dst[dstOff++] = rgb;
            }

            rgb = src[srcOff++];
            if (rgb == 0) {
                dstOff++;
            } else {
                dst[dstOff++] = rgb;
            }

            rgb = src[srcOff++];
            if (rgb == 0) {
                dstOff++;
            } else {
                dst[dstOff++] = rgb;
            }
        }

        for (int x = w; x < 0; x++) {
            int rgb = src[srcOff++];
            if (rgb == 0) {
                dstOff++;
            } else {
                dst[dstOff++] = rgb;
            }
        }

        dstOff += dstStep;
        srcOff += srcStep;
    }
}

void pix24_crop(Pix24 *pix24, int x, int y, int w, int h) {
    // try {
    int currentW = pix24->width;
    int currentH = pix24->height;

    int offW = 0;
    int offH = 0;
    int scaleWidth = (currentW << 16) / w;
    int scaleHeight = (currentH << 16) / h;
    (void)scaleWidth;
    (void)scaleHeight;

    int cw = pix24->crop_w;
    int ch = pix24->crop_h;
    int scaleCropWidth = (cw << 16) / w;
    int scaleCropHeight = (ch << 16) / h;

    x += (pix24->crop_x * w + cw - 1) / cw;
    y += (pix24->crop_y * h + ch - 1) / ch;

    if (pix24->crop_x * w % cw != 0) {
        offW = ((cw - pix24->crop_x * w % cw) << 16) / w;
    }

    if (pix24->crop_y * h % ch != 0) {
        offH = ((ch - pix24->crop_y * h % ch) << 16) / h;
    }

    w = w * (pix24->width - (offW >> 16)) / cw;
    h = h * (pix24->height - (offH >> 16)) / ch;

    int dstStep = x + y * _Pix2D.width;
    int dstOff = _Pix2D.width - w;

    if (y < _Pix2D.top) {
        int cutoff = _Pix2D.top - y;
        h -= cutoff;
        y = 0;
        dstStep += cutoff * _Pix2D.width;
        offH += scaleCropHeight * cutoff;
    }

    if (y + h > _Pix2D.bottom) {
        h -= y + h - _Pix2D.bottom;
    }

    if (x < _Pix2D.left) {
        int cutoff = _Pix2D.left - x;
        w -= cutoff;
        x = 0;
        dstStep += cutoff;
        offW += scaleCropWidth * cutoff;
        dstOff += cutoff;
    }

    if (x + w > _Pix2D.right) {
        int cutoff = x + w - _Pix2D.right;
        w -= cutoff;
        dstOff += cutoff;
    }

    pix24_scale(w, h, pix24->pixels, offW, offH, _Pix2D.pixels, dstOff, dstStep, currentW, scaleCropWidth, scaleCropHeight);
    // } catch (Exception ignored) {
    // 	System.out.println("error in sprite clipping routine");
    // }
}

void pix24_scale(int w, int h, int *src, int offW, int offH, int *dst, int dstStep, int dstOff, int currentW, int scaleCropWidth, int scaleCropHeight) {
    // try {
    int lastOffW = offW;

    for (int y = -h; y < 0; y++) {
        int offY = (offH >> 16) * currentW;

        for (int x = -w; x < 0; x++) {
            int rgb = src[(offW >> 16) + offY];
            if (rgb == 0) {
                dstOff++;
            } else {
                dst[dstOff++] = rgb;
            }

            offW += scaleCropWidth;
        }

        offH += scaleCropHeight;
        offW = lastOffW;
        dstOff += dstStep;
    }
    // } catch (Exception ignored) {
    // 	System.out.println("error in plot_scale");
    // }
}

void pix24_draw_alpha(Pix24 *pix24, int alpha, int x, int y) {
    x += pix24->crop_x;
    y += pix24->crop_y;

    int dstStep = x + y * _Pix2D.width;
    int srcStep = 0;
    int h = pix24->height;
    int w = pix24->width;
    int dstOff = _Pix2D.width - w;
    int srcOff = 0;

    if (y < _Pix2D.top) {
        int cutoff = _Pix2D.top - y;
        h -= cutoff;
        y = _Pix2D.top;
        srcStep += cutoff * w;
        dstStep += cutoff * _Pix2D.width;
    }

    if (y + h > _Pix2D.bottom) {
        h -= y + h - _Pix2D.bottom;
    }

    if (x < _Pix2D.left) {
        int cutoff = _Pix2D.left - x;
        w -= cutoff;
        x = _Pix2D.left;
        srcStep += cutoff;
        dstStep += cutoff;
        srcOff += cutoff;
        dstOff += cutoff;
    }

    if (x + w > _Pix2D.right) {
        int cutoff = x + w - _Pix2D.right;
        w -= cutoff;
        srcOff += cutoff;
        dstOff += cutoff;
    }

    if (w > 0 && h > 0) {
        pix24_copy_pixels_alpha(w, h, pix24->pixels, srcStep, srcOff, _Pix2D.pixels, dstStep, dstOff, alpha);
    }
}

void pix24_copy_pixels_alpha(int w, int h, int *src, int srcOff, int srcStep, int *dst, int dstOff, int dstStep, int alpha) {
    int invAlpha = 256 - alpha;

    for (int y = -h; y < 0; y++) {
        for (int x = -w; x < 0; x++) {
            int rgb = src[srcOff++];
            if (rgb == 0) {
                dstOff++;
            } else {
                int dstRgb = dst[dstOff];
                dst[dstOff++] = (((rgb & 0xff00ff) * alpha + (dstRgb & 0xff00ff) * invAlpha & 0xff00ff00) + ((rgb & 0xff00) * alpha + (dstRgb & 0xff00) * invAlpha & 0xff0000)) >> 8;
            }
        }

        dstOff += dstStep;
        srcOff += srcStep;
    }
}

void pix24_draw_rotated_masked(Pix24 *pix24, int x, int y, int w, int h, int *lineStart, int *lineWidth, int anchorX, int anchorY, int theta, int zoom) {
    // try {
    int centerX = -w / 2;
    int centerY = -h / 2;

    int sini = (int)(sin((double)theta / 326.11) * 65536.0);
    int cosi = (int)(cos((double)theta / 326.11) * 65536.0);
    int sinZoom = sini * zoom >> 8;
    int cosZoom = cosi * zoom >> 8;

    int leftX = (anchorX << 16) + centerY * sinZoom + centerX * cosZoom;
    int leftY = (anchorY << 16) + (centerY * cosZoom - centerX * sinZoom);
    int leftOff = x + y * _Pix2D.width;

    for (int i = 0; i < h; i++) {
        int dstOff = lineStart[i];
        int dstX = leftOff + dstOff;

        int srcX = leftX + cosZoom * dstOff;
        int srcY = leftY - sinZoom * dstOff;
        for (int j = -lineWidth[i]; j < 0; j++) {
            _Pix2D.pixels[dstX++] = pix24->pixels[(srcX >> 16) + (srcY >> 16) * pix24->width];
            srcX += cosZoom;
            srcY -= sinZoom;
        }

        leftX += sinZoom;
        leftY += cosZoom;
        leftOff += _Pix2D.width;
    }
    // } catch (Exception ignored) {
    // }
}

void pix24_draw_masked(Pix24 *pix24, int x, int y, Pix8 *mask) {
    x += pix24->crop_x;
    y += pix24->crop_y;

    int dstStep = x + y * _Pix2D.width;
    int srcStep = 0;
    int h = pix24->height;
    int w = pix24->width;
    int dstOff = _Pix2D.width - w;
    int srcOff = 0;

    if (y < _Pix2D.top) {
        int cutoff = _Pix2D.top - y;
        h -= cutoff;
        y = _Pix2D.top;
        srcStep += cutoff * w;
        dstStep += cutoff * _Pix2D.width;
    }

    if (y + h > _Pix2D.bottom) {
        h -= y + h - _Pix2D.bottom;
    }

    if (x < _Pix2D.left) {
        int cutoff = _Pix2D.left - x;
        w -= cutoff;
        x = _Pix2D.left;
        srcStep += cutoff;
        dstStep += cutoff;
        srcOff += cutoff;
        dstOff += cutoff;
    }

    if (x + w > _Pix2D.right) {
        int cutoff = x + w - _Pix2D.right;
        w -= cutoff;
        srcOff += cutoff;
        dstOff += cutoff;
    }

    if (w > 0 && h > 0) {
        pix24_copy_pixels_masked(w, h, pix24->pixels, srcOff, srcStep, _Pix2D.pixels, dstStep, dstOff, mask->pixels);
    }
}

void pix24_copy_pixels_masked(int w, int h, int *src, int srcStep, int srcOff, int *dst, int dstOff, int dstStep, int8_t *mask) {
    int qw = -(w >> 2);
    w = -(w & 0x3);

    for (int y = -h; y < 0; y++) {
        for (int x = qw; x < 0; x++) {
            int rgb = src[srcOff++];
            if (rgb != 0 && mask[dstOff] == 0) {
                dst[dstOff++] = rgb;
            } else {
                dstOff++;
            }

            rgb = src[srcOff++];
            if (rgb != 0 && mask[dstOff] == 0) {
                dst[dstOff++] = rgb;
            } else {
                dstOff++;
            }

            rgb = src[srcOff++];
            if (rgb != 0 && mask[dstOff] == 0) {
                dst[dstOff++] = rgb;
            } else {
                dstOff++;
            }

            rgb = src[srcOff++];
            if (rgb != 0 && mask[dstOff] == 0) {
                dst[dstOff++] = rgb;
            } else {
                dstOff++;
            }
        }

        for (int x = w; x < 0; x++) {
            int rgb = src[srcOff++];
            if (rgb != 0 && mask[dstOff] == 0) {
                dst[dstOff++] = rgb;
            } else {
                dstOff++;
            }
        }

        dstOff += dstStep;
        srcOff += srcStep;
    }
}
