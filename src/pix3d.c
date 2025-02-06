#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "pix3d.h"
#include "pix8.h"
#include "platform.h"

Pix3D _Pix3D = {.lowMemory = true, .jagged = true};
extern Pix2D _Pix2D;

static void gouraudRaster(int x0, int x1, int color0, int color1, int *dst, int offset, int length);
static void flatRaster(int x0, int x1, int *dst, int offset, int rgb);
static void textureRaster(int xA, int xB, int *dst, int offset, int *texels, int curU, int curV, int u, int v, int w, int uStride, int vStride, int wStride, int shadeA, int shadeB);

void pix3d_init_global(void) {
    _Pix3D.reciprical15 = malloc(512 * sizeof(int));
    _Pix3D.reciprical16 = malloc(2048 * sizeof(int));
    _Pix3D.sin_table = malloc(2048 * sizeof(int));
    _Pix3D.cos_table = malloc(2048 * sizeof(int));
    _Pix3D.renderWidth = 512;
    _Pix3D.renderHeight = 334;

    for (int i = 1; i < 512; i++) {
        _Pix3D.reciprical15[i] = 32768 / i;
    }
    for (int i = 1; i < 2048; i++) {
        _Pix3D.reciprical16[i] = 65536 / i;
    }
    for (int i = 0; i < 2048; i++) {
        _Pix3D.sin_table[i] = (int)(sin((double)i * 0.0030679615) * 65536.0);
        _Pix3D.cos_table[i] = (int)(cos((double)i * 0.0030679615) * 65536.0);
    }
    _Pix3D.textures = calloc(50, sizeof(Pix8 *));
    _Pix3D.textureHasTransparency = calloc(50, sizeof(bool));
    _Pix3D.textureColors = calloc(50, sizeof(int));
    _Pix3D.activeTexels = calloc(50, sizeof(int *));
    _Pix3D.textureCycle = calloc(50, sizeof(int));
    _Pix3D.palette = calloc(65536, sizeof(int));
    _Pix3D.texturePalettes = calloc(50, sizeof(int *));
}

void pix3d_free_global(void) {
    for (int i = 0; i < _Pix3D.poolSize; i++) {
        free(_Pix3D.texelPool[i]);
    }
    free(_Pix3D.texelPool);

    for (int i = 0; i < 50; i++) {
        pix8_free(_Pix3D.textures[i]);
        free(_Pix3D.texturePalettes[i]);
        free(_Pix3D.activeTexels[i]);
    }
    free(_Pix3D.textures);
    free(_Pix3D.texturePalettes);
    free(_Pix3D.activeTexels);
    // NOTE: line_offset doesn't own it's memory
    // free(_Pix3D.line_offset);
    free(_Pix3D.reciprical15);
    free(_Pix3D.reciprical16);
    free(_Pix3D.sin_table);
    free(_Pix3D.cos_table);
    free(_Pix3D.textureHasTransparency);
    free(_Pix3D.textureColors);
    free(_Pix3D.textureCycle);
    free(_Pix3D.palette);
}

void pix3d_init2d(void) {
    _Pix3D.line_offset = malloc(_Pix2D.height * sizeof(int));
    for (int y = 0; y < _Pix2D.height; y++) {
        _Pix3D.line_offset[y] = _Pix2D.width * y;
    }
    _Pix3D.center_x = _Pix2D.width / 2;
    _Pix3D.center_y = _Pix2D.height / 2;
}

void pix3d_init3d(int width, int height) {
    _Pix3D.line_offset = malloc(height * sizeof(int));
    for (int y = 0; y < height; y++) {
        _Pix3D.line_offset[y] = width * y;
    }
    _Pix3D.center_x = width / 2;
    _Pix3D.center_y = height / 2;
}

void pix3d_clear_texels(void) {
    for (int i = 0; i < _Pix3D.poolSize; i++) {
        free(_Pix3D.texelPool[i]);
        _Pix3D.texelPool[i] = NULL;
    }
    free(_Pix3D.texelPool);
    _Pix3D.texelPool = NULL;

    for (int i = 0; i < 50; i++) {
        free(_Pix3D.activeTexels[i]);
        _Pix3D.activeTexels[i] = NULL;
    }
}

void pix3d_init_pool(int size) {
    if (_Pix3D.texelPool) {
        return;
    }
    _Pix3D.poolSize = size;
    if (_Pix3D.lowMemory) {
        _Pix3D.texelPool = malloc(_Pix3D.poolSize * sizeof(int *));
        for (int i = 0; i < _Pix3D.poolSize; i++) {
            _Pix3D.texelPool[i] = calloc(16384, sizeof(int));
        }
    } else {
        _Pix3D.texelPool = malloc(_Pix3D.poolSize * sizeof(int *));
        for (int i = 0; i < _Pix3D.poolSize; i++) {
            _Pix3D.texelPool[i] = calloc(65536, sizeof(int));
        }
    }
    for (int i = 0; i < 50; i++) {
        _Pix3D.activeTexels[i] = NULL;
    }
}

void pix3d_unpack_textures(Jagfile *jag) {
    _Pix3D.textureCount = 0;
    for (int id = 0; id < 50; id++) {
        // try {
        char name[3];
        snprintf(name, sizeof(name), "%d", id);
        _Pix3D.textures[id] = pix8_from_archive(jag, name, 0);
        if (_Pix3D.lowMemory && _Pix3D.textures[id]->crop_w == 128) {
            pix8_shrink(_Pix3D.textures[id]);
        } else {
            pix8_crop(_Pix3D.textures[id]);
        }
        _Pix3D.textureCount++;
        // } catch (Exception ignored) {
    }
}

int pix3d_get_average_texture_rgb(int id) {
    if (_Pix3D.textureColors[id] != 0) {
        return _Pix3D.textureColors[id];
    }
    int r = 0;
    int g = 0;
    int b = 0;
    int length = _Pix3D.textures[id]->palette_count;
    for (int i = 0; i < length; i++) {
        r += _Pix3D.texturePalettes[id][i] >> 16 & 0xff;
        g += _Pix3D.texturePalettes[id][i] >> 8 & 0xff;
        b += _Pix3D.texturePalettes[id][i] & 0xff;
    }
    int rgb = (r / length << 16) + (g / length << 8) + b / length;
    rgb = pix3d_set_gamma(rgb, 1.4);
    if (rgb == 0) {
        rgb = 1;
    }
    _Pix3D.textureColors[id] = rgb;
    return rgb;
}

void pix3d_push_texture(int id) {
    if (_Pix3D.activeTexels[id]) {
        _Pix3D.texelPool[_Pix3D.poolSize++] = _Pix3D.activeTexels[id];
        _Pix3D.activeTexels[id] = NULL;
    }
}

static int *pix3d_get_texels(int id) {
    _Pix3D.textureCycle[id] = _Pix3D.cycle++;
    if (_Pix3D.activeTexels[id]) {
        return _Pix3D.activeTexels[id];
    }
    int *texels;
    if (_Pix3D.poolSize > 0) {
        texels = _Pix3D.texelPool[--_Pix3D.poolSize];
        _Pix3D.texelPool[_Pix3D.poolSize] = NULL;
    } else {
        int cycle = 0;
        int selected = -1;
        for (int t = 0; t < _Pix3D.textureCount; t++) {
            if (_Pix3D.activeTexels[t] && (_Pix3D.textureCycle[t] < cycle || selected == -1)) {
                cycle = _Pix3D.textureCycle[t];
                selected = t;
            }
        }
        texels = _Pix3D.activeTexels[selected];
        _Pix3D.activeTexels[selected] = NULL;
    }
    _Pix3D.activeTexels[id] = texels;
    Pix8 *texture = _Pix3D.textures[id];
    int *palette = _Pix3D.texturePalettes[id];
    if (_Pix3D.lowMemory) {
        _Pix3D.textureHasTransparency[id] = false;
        for (int i = 0; i < 4096; i++) {
            int rgb = texels[i] = palette[texture->pixels[i]] & 0xf8f8ff;
            if (rgb == 0) {
                _Pix3D.textureHasTransparency[id] = true;
            }
            texels[i + 4096] = rgb - ((uint32_t)rgb >> 3) & 0xf8f8ff;
            texels[i + 8192] = rgb - ((uint32_t)rgb >> 2) & 0xf8f8ff;
            texels[i + 12288] = rgb - ((uint32_t)rgb >> 2) - ((uint32_t)rgb >> 3) & 0xf8f8ff;
        }
    } else {
        if (texture->width == 64) {
            for (int y = 0; y < 128; y++) {
                for (int x = 0; x < 128; x++) {
                    texels[x + (y << 7)] = palette[texture->pixels[(x >> 1) + (y >> 1 << 6)]];
                }
            }
        } else {
            for (int i = 0; i < 16384; i++) {
                texels[i] = palette[texture->pixels[i]];
            }
        }
        _Pix3D.textureHasTransparency[id] = false;
        for (int i = 0; i < 16384; i++) {
            texels[i] &= 0xf8f8ff;
            int rgb = texels[i];
            if (rgb == 0) {
                _Pix3D.textureHasTransparency[id] = true;
            }
            texels[i + 16384] = rgb - ((uint32_t)rgb >> 3) & 0xf8f8ff;
            texels[i + 32768] = rgb - ((uint32_t)rgb >> 2) & 0xf8f8ff;
            texels[i + 49152] = rgb - ((uint32_t)rgb >> 2) - ((uint32_t)rgb >> 3) & 0xf8f8ff;
        }
    }
    return texels;
}

void pix3d_set_brightness(double brightness) {
    double random_brightness = brightness + jrand() * 0.03 - 0.015;
    int offset = 0;
    for (int y = 0; y < 512; y++) {
        double hue = (double)(y / 8) / 64.0 + 0.0078125;
        double saturation = (double)(y & 0x7) / 8.0 + 0.0625;
        for (int x = 0; x < 128; x++) {
            double lightness = (double)x / 128.0;
            double r = lightness;
            double g = lightness;
            double b = lightness;
            if (saturation != 0.0) {
                double q;
                if (lightness < 0.5) {
                    q = lightness * (saturation + 1.0);
                } else {
                    q = lightness + saturation - lightness * saturation;
                }
                double p = lightness * 2.0 - q;
                double t = hue + 0.3333333333333333;
                if (t > 1.0) {
                    t--;
                }
                double d11 = hue - 0.3333333333333333;
                if (d11 < 0.0) {
                    d11++;
                }
                if (t * 6.0 < 1.0) {
                    r = p + (q - p) * 6.0 * t;
                } else if (t * 2.0 < 1.0) {
                    r = q;
                } else if (t * 3.0 < 2.0) {
                    r = p + (q - p) * (0.6666666666666666 - t) * 6.0;
                } else {
                    r = p;
                }
                if (hue * 6.0 < 1.0) {
                    g = p + (q - p) * 6.0 * hue;
                } else if (hue * 2.0 < 1.0) {
                    g = q;
                } else if (hue * 3.0 < 2.0) {
                    g = p + (q - p) * (0.6666666666666666 - hue) * 6.0;
                } else {
                    g = p;
                }
                if (d11 * 6.0 < 1.0) {
                    b = p + (q - p) * 6.0 * d11;
                } else if (d11 * 2.0 < 1.0) {
                    b = q;
                } else if (d11 * 3.0 < 2.0) {
                    b = p + (q - p) * (0.6666666666666666 - d11) * 6.0;
                } else {
                    b = p;
                }
            }
            int intR = (int)(r * 256.0);
            int intG = (int)(g * 256.0);
            int intB = (int)(b * 256.0);
            int rgb = (intR << 16) + (intG << 8) + intB;
            int rgbAdjusted = pix3d_set_gamma(rgb, random_brightness);
            _Pix3D.palette[offset++] = rgbAdjusted;
        }
    }
    for (int id = 0; id < 50; id++) {
        if (_Pix3D.textures[id]) {
            int palette_count = _Pix3D.textures[id]->palette_count;
            int *palette = _Pix3D.textures[id]->palette;
            _Pix3D.texturePalettes[id] = calloc(palette_count, sizeof(int));
            for (int i = 0; i < palette_count; i++) {
                _Pix3D.texturePalettes[id][i] = pix3d_set_gamma(palette[i], random_brightness);
            }
        }
    }
    for (int id = 0; id < 50; id++) {
        pix3d_push_texture(id);
    }
}

int pix3d_set_gamma(int rgb, double gamma) {
    double r = (double)(rgb >> 16) / 256.0;
    double g = (double)(rgb >> 8 & 0xff) / 256.0;
    double b = (double)(rgb & 0xff) / 256.0;
    double powR = pow(r, gamma);
    double powG = pow(g, gamma);
    double powB = pow(b, gamma);
    int intR = (int)(powR * 256.0);
    int intG = (int)(powG * 256.0);
    int intB = (int)(powB * 256.0);
    return (intR << 16) + (intG << 8) + intB;
}

void gouraudTriangle(int xA, int xB, int xC, int yA, int yB, int yC, int colorA, int colorB, int colorC) {
    int dxAB = xB - xA;
    int dyAB = yB - yA;
    int dxAC = xC - xA;
    int dyAC = yC - yA;

    int xStepAB = 0;
    int colorStepAB = 0;
    if (yB != yA) {
        xStepAB = (dxAB << 16) / dyAB;
        colorStepAB = ((colorB - colorA) << 15) / dyAB;
    }

    int xStepBC = 0;
    int colorStepBC = 0;
    if (yC != yB) {
        xStepBC = ((xC - xB) << 16) / (yC - yB);
        colorStepBC = ((colorC - colorB) << 15) / (yC - yB);
    }

    int xStepAC = 0;
    int colorStepAC = 0;
    if (yC != yA) {
        xStepAC = ((xA - xC) << 16) / (yA - yC);
        colorStepAC = ((colorA - colorC) << 15) / (yA - yC);
    }

    // this won't change any rendering, saves not wasting time "drawing" an invalid triangle
    int triangleArea = (dxAB * dyAC) - (dyAB * dxAC);
    if (triangleArea == 0) {
        return;
    }

    if (yA <= yB && yA <= yC) {
        if (yA < _Pix2D.bottom) {
            if (yB > _Pix2D.bottom) {
                yB = _Pix2D.bottom;
            }

            if (yC > _Pix2D.bottom) {
                yC = _Pix2D.bottom;
            }

            if (yB < yC) {
                xC = xA <<= 16;
                colorC = colorA <<= 15;
                if (yA < 0) {
                    xC -= xStepAC * yA;
                    xA -= xStepAB * yA;
                    colorC -= colorStepAC * yA;
                    colorA -= colorStepAB * yA;
                    yA = 0;
                }

                xB <<= 16;
                colorB <<= 15;
                if (yB < 0) {
                    xB -= xStepBC * yB;
                    colorB -= colorStepBC * yB;
                    yB = 0;
                }

                if ((yA != yB && xStepAC < xStepAB) || (yA == yB && xStepAC > xStepBC)) {
                    yC -= yB;
                    yB -= yA;
                    yA = _Pix3D.line_offset[yA];

                    while (--yB >= 0) {
                        gouraudRaster(xC >> 16, xA >> 16, colorC >> 7, colorA >> 7, _Pix2D.pixels, yA, 0);
                        xC += xStepAC;
                        xA += xStepAB;
                        colorC += colorStepAC;
                        colorA += colorStepAB;
                        yA += _Pix2D.width;
                    }
                    while (--yC >= 0) {
                        gouraudRaster(xC >> 16, xB >> 16, colorC >> 7, colorB >> 7, _Pix2D.pixels, yA, 0);
                        xC += xStepAC;
                        xB += xStepBC;
                        colorC += colorStepAC;
                        colorB += colorStepBC;
                        yA += _Pix2D.width;
                    }
                } else {
                    yC -= yB;
                    yB -= yA;
                    yA = _Pix3D.line_offset[yA];

                    while (--yB >= 0) {
                        gouraudRaster(xA >> 16, xC >> 16, colorA >> 7, colorC >> 7, _Pix2D.pixels, yA, 0);
                        xC += xStepAC;
                        xA += xStepAB;
                        colorC += colorStepAC;
                        colorA += colorStepAB;
                        yA += _Pix2D.width;
                    }
                    while (--yC >= 0) {
                        gouraudRaster(xB >> 16, xC >> 16, colorB >> 7, colorC >> 7, _Pix2D.pixels, yA, 0);
                        xC += xStepAC;
                        xB += xStepBC;
                        colorC += colorStepAC;
                        colorB += colorStepBC;
                        yA += _Pix2D.width;
                    }
                }
            } else {
                xB = xA <<= 16;
                colorB = colorA <<= 15;
                if (yA < 0) {
                    xB -= xStepAC * yA;
                    xA -= xStepAB * yA;
                    colorB -= colorStepAC * yA;
                    colorA -= colorStepAB * yA;
                    yA = 0;
                }

                xC <<= 16;
                colorC <<= 15;
                if (yC < 0) {
                    xC -= xStepBC * yC;
                    colorC -= colorStepBC * yC;
                    yC = 0;
                }

                if ((yA != yC && xStepAC < xStepAB) || (yA == yC && xStepBC > xStepAB)) {
                    yB -= yC;
                    yC -= yA;
                    yA = _Pix3D.line_offset[yA];

                    while (--yC >= 0) {
                        gouraudRaster(xB >> 16, xA >> 16, colorB >> 7, colorA >> 7, _Pix2D.pixels, yA, 0);
                        xB += xStepAC;
                        xA += xStepAB;
                        colorB += colorStepAC;
                        colorA += colorStepAB;
                        yA += _Pix2D.width;
                    }
                    while (--yB >= 0) {
                        gouraudRaster(xC >> 16, xA >> 16, colorC >> 7, colorA >> 7, _Pix2D.pixels, yA, 0);
                        xC += xStepBC;
                        xA += xStepAB;
                        colorC += colorStepBC;
                        colorA += colorStepAB;
                        yA += _Pix2D.width;
                    }
                } else {
                    yB -= yC;
                    yC -= yA;
                    yA = _Pix3D.line_offset[yA];

                    while (--yC >= 0) {
                        gouraudRaster(xA >> 16, xB >> 16, colorA >> 7, colorB >> 7, _Pix2D.pixels, yA, 0);
                        xB += xStepAC;
                        xA += xStepAB;
                        colorB += colorStepAC;
                        colorA += colorStepAB;
                        yA += _Pix2D.width;
                    }
                    while (--yB >= 0) {
                        gouraudRaster(xA >> 16, xC >> 16, colorA >> 7, colorC >> 7, _Pix2D.pixels, yA, 0);
                        xC += xStepBC;
                        xA += xStepAB;
                        colorC += colorStepBC;
                        colorA += colorStepAB;
                        yA += _Pix2D.width;
                    }
                }
            }
        }
    } else if (yB <= yC) {
        if (yB < _Pix2D.bottom) {
            if (yC > _Pix2D.bottom) {
                yC = _Pix2D.bottom;
            }

            if (yA > _Pix2D.bottom) {
                yA = _Pix2D.bottom;
            }

            if (yC < yA) {
                xA = xB <<= 16;
                colorA = colorB <<= 15;
                if (yB < 0) {
                    xA -= xStepAB * yB;
                    xB -= xStepBC * yB;
                    colorA -= colorStepAB * yB;
                    colorB -= colorStepBC * yB;
                    yB = 0;
                }

                xC <<= 16;
                colorC <<= 15;
                if (yC < 0) {
                    xC -= xStepAC * yC;
                    colorC -= colorStepAC * yC;
                    yC = 0;
                }

                if ((yB != yC && xStepAB < xStepBC) || (yB == yC && xStepAB > xStepAC)) {
                    yA -= yC;
                    yC -= yB;
                    yB = _Pix3D.line_offset[yB];

                    while (--yC >= 0) {
                        gouraudRaster(xA >> 16, xB >> 16, colorA >> 7, colorB >> 7, _Pix2D.pixels, yB, 0);
                        xA += xStepAB;
                        xB += xStepBC;
                        colorA += colorStepAB;
                        colorB += colorStepBC;
                        yB += _Pix2D.width;
                    }
                    while (--yA >= 0) {
                        gouraudRaster(xA >> 16, xC >> 16, colorA >> 7, colorC >> 7, _Pix2D.pixels, yB, 0);
                        xA += xStepAB;
                        xC += xStepAC;
                        colorA += colorStepAB;
                        colorC += colorStepAC;
                        yB += _Pix2D.width;
                    }
                } else {
                    yA -= yC;
                    yC -= yB;
                    yB = _Pix3D.line_offset[yB];

                    while (--yC >= 0) {
                        gouraudRaster(xB >> 16, xA >> 16, colorB >> 7, colorA >> 7, _Pix2D.pixels, yB, 0);
                        xA += xStepAB;
                        xB += xStepBC;
                        colorA += colorStepAB;
                        colorB += colorStepBC;
                        yB += _Pix2D.width;
                    }
                    while (--yA >= 0) {
                        gouraudRaster(xC >> 16, xA >> 16, colorC >> 7, colorA >> 7, _Pix2D.pixels, yB, 0);
                        xA += xStepAB;
                        xC += xStepAC;
                        colorA += colorStepAB;
                        colorC += colorStepAC;
                        yB += _Pix2D.width;
                    }
                }
            } else {
                xC = xB <<= 16;
                colorC = colorB <<= 15;
                if (yB < 0) {
                    xC -= xStepAB * yB;
                    xB -= xStepBC * yB;
                    colorC -= colorStepAB * yB;
                    colorB -= colorStepBC * yB;
                    yB = 0;
                }

                xA <<= 16;
                colorA <<= 15;
                if (yA < 0) {
                    xA -= xStepAC * yA;
                    colorA -= colorStepAC * yA;
                    yA = 0;
                }

                if (xStepAB < xStepBC) {
                    yC -= yA;
                    yA -= yB;
                    yB = _Pix3D.line_offset[yB];

                    while (--yA >= 0) {
                        gouraudRaster(xC >> 16, xB >> 16, colorC >> 7, colorB >> 7, _Pix2D.pixels, yB, 0);
                        xC += xStepAB;
                        xB += xStepBC;
                        colorC += colorStepAB;
                        colorB += colorStepBC;
                        yB += _Pix2D.width;
                    }
                    while (--yC >= 0) {
                        gouraudRaster(xA >> 16, xB >> 16, colorA >> 7, colorB >> 7, _Pix2D.pixels, yB, 0);
                        xA += xStepAC;
                        xB += xStepBC;
                        colorA += colorStepAC;
                        colorB += colorStepBC;
                        yB += _Pix2D.width;
                    }
                } else {
                    yC -= yA;
                    yA -= yB;
                    yB = _Pix3D.line_offset[yB];

                    while (--yA >= 0) {
                        gouraudRaster(xB >> 16, xC >> 16, colorB >> 7, colorC >> 7, _Pix2D.pixels, yB, 0);
                        xC += xStepAB;
                        xB += xStepBC;
                        colorC += colorStepAB;
                        colorB += colorStepBC;
                        yB += _Pix2D.width;
                    }
                    while (--yC >= 0) {
                        gouraudRaster(xB >> 16, xA >> 16, colorB >> 7, colorA >> 7, _Pix2D.pixels, yB, 0);
                        xA += xStepAC;
                        xB += xStepBC;
                        colorA += colorStepAC;
                        colorB += colorStepBC;
                        yB += _Pix2D.width;
                    }
                }
            }
        }
    } else if (yC < _Pix2D.bottom) {
        if (yA > _Pix2D.bottom) {
            yA = _Pix2D.bottom;
        }

        if (yB > _Pix2D.bottom) {
            yB = _Pix2D.bottom;
        }

        if (yA < yB) {
            xB = xC <<= 16;
            colorB = colorC <<= 15;
            if (yC < 0) {
                xB -= xStepBC * yC;
                xC -= xStepAC * yC;
                colorB -= colorStepBC * yC;
                colorC -= colorStepAC * yC;
                yC = 0;
            }

            xA <<= 16;
            colorA <<= 15;
            if (yA < 0) {
                xA -= xStepAB * yA;
                colorA -= colorStepAB * yA;
                yA = 0;
            }

            if (xStepBC < xStepAC) {
                yB -= yA;
                yA -= yC;
                yC = _Pix3D.line_offset[yC];

                while (--yA >= 0) {
                    gouraudRaster(xB >> 16, xC >> 16, colorB >> 7, colorC >> 7, _Pix2D.pixels, yC, 0);
                    xB += xStepBC;
                    xC += xStepAC;
                    colorB += colorStepBC;
                    colorC += colorStepAC;
                    yC += _Pix2D.width;
                }
                while (--yB >= 0) {
                    gouraudRaster(xB >> 16, xA >> 16, colorB >> 7, colorA >> 7, _Pix2D.pixels, yC, 0);
                    xB += xStepBC;
                    xA += xStepAB;
                    colorB += colorStepBC;
                    colorA += colorStepAB;
                    yC += _Pix2D.width;
                }
            } else {
                yB -= yA;
                yA -= yC;
                yC = _Pix3D.line_offset[yC];

                while (--yA >= 0) {
                    gouraudRaster(xC >> 16, xB >> 16, colorC >> 7, colorB >> 7, _Pix2D.pixels, yC, 0);
                    xB += xStepBC;
                    xC += xStepAC;
                    colorB += colorStepBC;
                    colorC += colorStepAC;
                    yC += _Pix2D.width;
                }
                while (--yB >= 0) {
                    gouraudRaster(xA >> 16, xB >> 16, colorA >> 7, colorB >> 7, _Pix2D.pixels, yC, 0);
                    xB += xStepBC;
                    xA += xStepAB;
                    colorB += colorStepBC;
                    colorA += colorStepAB;
                    yC += _Pix2D.width;
                }
            }
        } else {
            xA = xC <<= 16;
            colorA = colorC <<= 15;
            if (yC < 0) {
                xA -= xStepBC * yC;
                xC -= xStepAC * yC;
                colorA -= colorStepBC * yC;
                colorC -= colorStepAC * yC;
                yC = 0;
            }

            xB <<= 16;
            colorB <<= 15;
            if (yB < 0) {
                xB -= xStepAB * yB;
                colorB -= colorStepAB * yB;
                yB = 0;
            }

            if (xStepBC < xStepAC) {
                yA -= yB;
                yB -= yC;
                yC = _Pix3D.line_offset[yC];

                while (--yB >= 0) {
                    gouraudRaster(xA >> 16, xC >> 16, colorA >> 7, colorC >> 7, _Pix2D.pixels, yC, 0);
                    xA += xStepBC;
                    xC += xStepAC;
                    colorA += colorStepBC;
                    colorC += colorStepAC;
                    yC += _Pix2D.width;
                }
                while (--yA >= 0) {
                    gouraudRaster(xB >> 16, xC >> 16, colorB >> 7, colorC >> 7, _Pix2D.pixels, yC, 0);
                    xB += xStepAB;
                    xC += xStepAC;
                    colorB += colorStepAB;
                    colorC += colorStepAC;
                    yC += _Pix2D.width;
                }
            } else {
                yA -= yB;
                yB -= yC;
                yC = _Pix3D.line_offset[yC];

                while (--yB >= 0) {
                    gouraudRaster(xC >> 16, xA >> 16, colorC >> 7, colorA >> 7, _Pix2D.pixels, yC, 0);
                    xA += xStepBC;
                    xC += xStepAC;
                    colorA += colorStepBC;
                    colorC += colorStepAC;
                    yC += _Pix2D.width;
                }
                while (--yA >= 0) {
                    gouraudRaster(xC >> 16, xB >> 16, colorC >> 7, colorB >> 7, _Pix2D.pixels, yC, 0);
                    xB += xStepAB;
                    xC += xStepAC;
                    colorB += colorStepAB;
                    colorC += colorStepAC;
                    yC += _Pix2D.width;
                }
            }
        }
    }
}

void gouraudRaster(int x0, int x1, int color0, int color1, int *dst, int offset, int length) {
    int rgb;

    if (_Pix3D.jagged) {
        int colorStep;

        if (_Pix3D.clipX) {
            if (x1 - x0 > 3) {
                colorStep = (color1 - color0) / (x1 - x0);
            } else {
                colorStep = 0;
            }

            if (x1 > _Pix2D.bound_x) {
                x1 = _Pix2D.bound_x;
            }

            if (x0 < 0) {
                color0 -= x0 * colorStep;
                x0 = 0;
            }

            if (x0 >= x1) {
                return;
            }

            offset += x0;
            length = (x1 - x0) >> 2;
            colorStep <<= 2;
        } else if (x0 < x1) {
            offset += x0;
            length = (x1 - x0) >> 2;

            if (length > 0) {
                colorStep = (color1 - color0) * _Pix3D.reciprical15[length] >> 15;
            } else {
                colorStep = 0;
            }
        } else {
            return;
        }

        if (_Pix3D.alpha == 0) {
            while (--length >= 0) {
                rgb = _Pix3D.palette[color0 >> 8];
                color0 += colorStep;

                dst[offset++] = rgb;
                dst[offset++] = rgb;
                dst[offset++] = rgb;
                dst[offset++] = rgb;
            }

            length = (x1 - x0) & 0x3;
            if (length > 0) {
                rgb = _Pix3D.palette[color0 >> 8];

                while (--length >= 0) {
                    dst[offset++] = rgb;
                }
            }
        } else {
            int alpha = _Pix3D.alpha;
            int invAlpha = 256 - _Pix3D.alpha;

            while (--length >= 0) {
                rgb = _Pix3D.palette[color0 >> 8];
                color0 += colorStep;

                rgb = ((((rgb & 0xff00ff) * invAlpha) >> 8) & 0xff00ff) + ((((rgb & 0xff00) * invAlpha) >> 8) & 0xff00);
                dst[offset] = rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00);
                offset++;
                dst[offset] = rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00);
                offset++;
                dst[offset] = rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00);
                offset++;
                dst[offset] = rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00);
                offset++;
            }

            length = (x1 - x0) & 0x3;
            if (length > 0) {
                rgb = _Pix3D.palette[color0 >> 8];
                rgb = ((((rgb & 0xff00ff) * invAlpha) >> 8) & 0xff00ff) + ((((rgb & 0xff00) * invAlpha) >> 8) & 0xff00);

                while (--length >= 0) {
                    dst[offset] = rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00);
                    offset++;
                }
            }
        }
    } else if (x0 < x1) {
        int colorStep = (color1 - color0) / (x1 - x0);

        if (_Pix3D.clipX) {
            if (x1 > _Pix2D.bound_x) {
                x1 = _Pix2D.bound_x;
            }

            if (x0 < 0) {
                color0 -= x0 * colorStep;
                x0 = 0;
            }

            if (x0 >= x1) {
                return;
            }
        }

        offset += x0;
        length = x1 - x0;

        if (_Pix3D.alpha == 0) {
            while (--length >= 0) {
                dst[offset++] = _Pix3D.palette[color0 >> 8];
                color0 += colorStep;
            }
        } else {
            int alpha = _Pix3D.alpha;
            int invAlpha = 256 - _Pix3D.alpha;

            while (--length >= 0) {
                rgb = _Pix3D.palette[color0 >> 8];
                color0 += colorStep;

                rgb = ((((rgb & 0xff00ff) * invAlpha) >> 8) & 0xff00ff) + ((((rgb & 0xff00) * invAlpha) >> 8) & 0xff00);
                dst[offset] = rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00);
                offset++;
            }
        }
    }
}

void flatTriangle(int xA, int xB, int xC, int yA, int yB, int yC, int color) {
    int dxAB = xB - xA;
    int dyAB = yB - yA;
    int dxAC = xC - xA;
    int dyAC = yC - yA;

    int xStepAB = 0;
    if (yB != yA) {
        xStepAB = (dxAB << 16) / dyAB;
    }

    int xStepBC = 0;
    if (yC != yB) {
        xStepBC = ((xC - xB) << 16) / (yC - yB);
    }

    int xStepAC = 0;
    if (yC != yA) {
        xStepAC = ((xA - xC) << 16) / (yA - yC);
    }

    // this won't change any rendering, saves not wasting time "drawing" an invalid triangle
    int triangleArea = (dxAB * dyAC) - (dyAB * dxAC);
    if (triangleArea == 0) {
        return;
    }

    if (yA <= yB && yA <= yC) {
        if (yA < _Pix2D.bottom) {
            if (yB > _Pix2D.bottom) {
                yB = _Pix2D.bottom;
            }

            if (yC > _Pix2D.bottom) {
                yC = _Pix2D.bottom;
            }

            if (yB < yC) {
                xC = xA <<= 16;
                if (yA < 0) {
                    xC -= xStepAC * yA;
                    xA -= xStepAB * yA;
                    yA = 0;
                }

                xB <<= 16;
                if (yB < 0) {
                    xB -= xStepBC * yB;
                    yB = 0;
                }

                if ((yA != yB && xStepAC < xStepAB) || (yA == yB && xStepAC > xStepBC)) {
                    yC -= yB;
                    yB -= yA;
                    yA = _Pix3D.line_offset[yA];

                    while (--yB >= 0) {
                        flatRaster(xC >> 16, xA >> 16, _Pix2D.pixels, yA, color);
                        xC += xStepAC;
                        xA += xStepAB;
                        yA += _Pix2D.width;
                    }
                    while (--yC >= 0.0F) {
                        flatRaster(xC >> 16, xB >> 16, _Pix2D.pixels, yA, color);
                        xC += xStepAC;
                        xB += xStepBC;
                        yA += _Pix2D.width;
                    }
                } else {
                    yC -= yB;
                    yB -= yA;
                    yA = _Pix3D.line_offset[yA];

                    while (--yB >= 0) {
                        flatRaster(xA >> 16, xC >> 16, _Pix2D.pixels, yA, color);
                        xC += xStepAC;
                        xA += xStepAB;
                        yA += _Pix2D.width;
                    }
                    while (--yC >= 0) {
                        flatRaster(xB >> 16, xC >> 16, _Pix2D.pixels, yA, color);
                        xC += xStepAC;
                        xB += xStepBC;
                        yA += _Pix2D.width;
                    }
                }
            } else {
                xB = xA <<= 16;
                if (yA < 0) {
                    xB -= xStepAC * yA;
                    xA -= xStepAB * yA;
                    yA = 0;
                }

                xC <<= 16;
                if (yC < 0) {
                    xC -= xStepBC * yC;
                    yC = 0;
                }

                if ((yA != yC && xStepAC < xStepAB) || (yA == yC && xStepBC > xStepAB)) {
                    yB -= yC;
                    yC -= yA;
                    yA = _Pix3D.line_offset[yA];

                    while (--yC >= 0) {
                        flatRaster(xB >> 16, xA >> 16, _Pix2D.pixels, yA, color);
                        xB += xStepAC;
                        xA += xStepAB;
                        yA += _Pix2D.width;
                    }
                    while (--yB >= 0) {
                        flatRaster(xC >> 16, xA >> 16, _Pix2D.pixels, yA, color);
                        xC += xStepBC;
                        xA += xStepAB;
                        yA += _Pix2D.width;
                    }
                } else {
                    yB -= yC;
                    yC -= yA;
                    yA = _Pix3D.line_offset[yA];

                    while (--yC >= 0) {
                        flatRaster(xA >> 16, xB >> 16, _Pix2D.pixels, yA, color);
                        xB += xStepAC;
                        xA += xStepAB;
                        yA += _Pix2D.width;
                    }
                    while (--yB >= 0) {
                        flatRaster(xA >> 16, xC >> 16, _Pix2D.pixels, yA, color);
                        xC += xStepBC;
                        xA += xStepAB;
                        yA += _Pix2D.width;
                    }
                }
            }
        }
    } else if (yB <= yC) {
        if (yB < _Pix2D.bottom) {
            if (yC > _Pix2D.bottom) {
                yC = _Pix2D.bottom;
            }

            if (yA > _Pix2D.bottom) {
                yA = _Pix2D.bottom;
            }

            if (yC < yA) {
                xA = xB <<= 16;
                if (yB < 0) {
                    xA -= xStepAB * yB;
                    xB -= xStepBC * yB;
                    yB = 0;
                }

                xC <<= 16;
                if (yC < 0) {
                    xC -= xStepAC * yC;
                    yC = 0;
                }

                if ((yB != yC && xStepAB < xStepBC) || (yB == yC && xStepAB > xStepAC)) {
                    yA -= yC;
                    yC -= yB;
                    yB = _Pix3D.line_offset[yB];

                    while (--yC >= 0) {
                        flatRaster(xA >> 16, xB >> 16, _Pix2D.pixels, yB, color);
                        xA += xStepAB;
                        xB += xStepBC;
                        yB += _Pix2D.width;
                    }
                    while (--yA >= 0) {
                        flatRaster(xA >> 16, xC >> 16, _Pix2D.pixels, yB, color);
                        xA += xStepAB;
                        xC += xStepAC;
                        yB += _Pix2D.width;
                    }
                } else {
                    yA -= yC;
                    yC -= yB;
                    yB = _Pix3D.line_offset[yB];

                    while (--yC >= 0) {
                        flatRaster(xB >> 16, xA >> 16, _Pix2D.pixels, yB, color);
                        xA += xStepAB;
                        xB += xStepBC;
                        yB += _Pix2D.width;
                    }
                    while (--yA >= 0) {
                        flatRaster(xC >> 16, xA >> 16, _Pix2D.pixels, yB, color);
                        xA += xStepAB;
                        xC += xStepAC;
                        yB += _Pix2D.width;
                    }
                }
            } else {
                xC = xB <<= 16;
                if (yB < 0) {
                    xC -= xStepAB * yB;
                    xB -= xStepBC * yB;
                    yB = 0;
                }

                xA <<= 16;
                if (yA < 0) {
                    xA -= xStepAC * yA;
                    yA = 0;
                }

                if (xStepAB < xStepBC) {
                    yC -= yA;
                    yA -= yB;
                    yB = _Pix3D.line_offset[yB];

                    while (--yA >= 0) {
                        flatRaster(xC >> 16, xB >> 16, _Pix2D.pixels, yB, color);
                        xC += xStepAB;
                        xB += xStepBC;
                        yB += _Pix2D.width;
                    }
                    while (--yC >= 0) {
                        flatRaster(xA >> 16, xB >> 16, _Pix2D.pixels, yB, color);
                        xA += xStepAC;
                        xB += xStepBC;
                        yB += _Pix2D.width;
                    }
                } else {
                    yC -= yA;
                    yA -= yB;
                    yB = _Pix3D.line_offset[yB];

                    while (--yA >= 0) {
                        flatRaster(xB >> 16, xC >> 16, _Pix2D.pixels, yB, color);
                        xC += xStepAB;
                        xB += xStepBC;
                        yB += _Pix2D.width;
                    }
                    while (--yC >= 0) {
                        flatRaster(xB >> 16, xA >> 16, _Pix2D.pixels, yB, color);
                        xA += xStepAC;
                        xB += xStepBC;
                        yB += _Pix2D.width;
                    }
                }
            }
        }
    } else if (yC < _Pix2D.bottom) {
        if (yA > _Pix2D.bottom) {
            yA = _Pix2D.bottom;
        }

        if (yB > _Pix2D.bottom) {
            yB = _Pix2D.bottom;
        }

        if (yA < yB) {
            xB = xC <<= 16;
            if (yC < 0) {
                xB -= xStepBC * yC;
                xC -= xStepAC * yC;
                yC = 0;
            }

            xA <<= 16;
            if (yA < 0) {
                xA -= xStepAB * yA;
                yA = 0;
            }

            if (xStepBC < xStepAC) {
                yB -= yA;
                yA -= yC;
                yC = _Pix3D.line_offset[yC];

                while (--yA >= 0) {
                    flatRaster(xB >> 16, xC >> 16, _Pix2D.pixels, yC, color);
                    xB += xStepBC;
                    xC += xStepAC;
                    yC += _Pix2D.width;
                }
                while (--yB >= 0) {
                    flatRaster(xB >> 16, xA >> 16, _Pix2D.pixels, yC, color);
                    xB += xStepBC;
                    xA += xStepAB;
                    yC += _Pix2D.width;
                }
            } else {
                yB -= yA;
                yA -= yC;
                yC = _Pix3D.line_offset[yC];

                while (--yA >= 0) {
                    flatRaster(xC >> 16, xB >> 16, _Pix2D.pixels, yC, color);
                    xB += xStepBC;
                    xC += xStepAC;
                    yC += _Pix2D.width;
                }
                while (--yB >= 0) {
                    flatRaster(xA >> 16, xB >> 16, _Pix2D.pixels, yC, color);
                    xB += xStepBC;
                    xA += xStepAB;
                    yC += _Pix2D.width;
                }
            }
        } else {
            xA = xC <<= 16;
            if (yC < 0) {
                xA -= xStepBC * yC;
                xC -= xStepAC * yC;
                yC = 0;
            }

            xB <<= 16;
            if (yB < 0) {
                xB -= xStepAB * yB;
                yB = 0;
            }

            if (xStepBC < xStepAC) {
                yA -= yB;
                yB -= yC;
                yC = _Pix3D.line_offset[yC];

                while (--yB >= 0) {
                    flatRaster(xA >> 16, xC >> 16, _Pix2D.pixels, yC, color);
                    xA += xStepBC;
                    xC += xStepAC;
                    yC += _Pix2D.width;
                }
                while (--yA >= 0) {
                    flatRaster(xB >> 16, xC >> 16, _Pix2D.pixels, yC, color);
                    xB += xStepAB;
                    xC += xStepAC;
                    yC += _Pix2D.width;
                }
            } else {
                yA -= yB;
                yB -= yC;
                yC = _Pix3D.line_offset[yC];

                while (--yB >= 0) {
                    flatRaster(xC >> 16, xA >> 16, _Pix2D.pixels, yC, color);
                    xA += xStepBC;
                    xC += xStepAC;
                    yC += _Pix2D.width;
                }
                while (--yA >= 0) {
                    flatRaster(xC >> 16, xB >> 16, _Pix2D.pixels, yC, color);
                    xB += xStepAB;
                    xC += xStepAC;
                    yC += _Pix2D.width;
                }
            }
        }
    }
}

void flatRaster(int x0, int x1, int *dst, int offset, int rgb) {
    if (_Pix3D.clipX) {
        if (x1 > _Pix2D.bound_x) {
            x1 = _Pix2D.bound_x;
        }

        if (x0 < 0) {
            x0 = 0;
        }
    }

    if (x0 >= x1) {
        return;
    }

    offset += x0;
    int length = (x1 - x0) >> 2;

    if (_Pix3D.alpha == 0) {
        while (--length >= 0) {
            dst[offset++] = rgb;
            dst[offset++] = rgb;
            dst[offset++] = rgb;
            dst[offset++] = rgb;
        }

        length = (x1 - x0) & 0x3;
        while (--length >= 0) {
            dst[offset++] = rgb;
        }
    } else {
        int alpha = _Pix3D.alpha;
        int invAlpha = 256 - _Pix3D.alpha;
        rgb = ((rgb & 0xff00ff) * invAlpha >> 8 & 0xff00ff) + ((rgb & 0xff00) * invAlpha >> 8 & 0xff00);

        while (--length >= 0) {
            dst[offset] = rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00);
            offset++;
            dst[offset] = rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00);
            offset++;
            dst[offset] = rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00);
            offset++;
            dst[offset] = rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00);
            offset++;
        }

        length = (x1 - x0) & 0x3;
        while (--length >= 0) {
            dst[offset] = rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00);
            offset++;
        }
    }
}

void textureTriangle(int xA, int xB, int xC, int yA, int yB, int yC, int shadeA, int shadeB, int shadeC, int originX, int originY, int originZ, int txB, int txC, int tyB, int tyC, int tzB, int tzC, int texture) {
    int *texels = pix3d_get_texels(texture);
    _Pix3D.opaque = !_Pix3D.textureHasTransparency[texture];

    int verticalX = originX - txB;
    int verticalY = originY - tyB;
    int verticalZ = originZ - tzB;

    int horizontalX = txC - originX;
    int horizontalY = tyC - originY;
    int horizontalZ = tzC - originZ;

    int u = ((horizontalX * originY) - (horizontalY * originX)) << 14;
    int uStride = ((horizontalY * originZ) - (horizontalZ * originY)) << 8;
    int uStepVertical = ((horizontalZ * originX) - (horizontalX * originZ)) << 5;

    int v = ((verticalX * originY) - (verticalY * originX)) << 14;
    int vStride = ((verticalY * originZ) - (verticalZ * originY)) << 8;
    int vStepVertical = ((verticalZ * originX) - (verticalX * originZ)) << 5;

    int w = ((verticalY * horizontalX) - (verticalX * horizontalY)) << 14;
    int wStride = ((verticalZ * horizontalY) - (verticalY * horizontalZ)) << 8;
    int wStepVertical = ((verticalX * horizontalZ) - (verticalZ * horizontalX)) << 5;

    int dxAB = xB - xA;
    int dyAB = yB - yA;
    int dxAC = xC - xA;
    int dyAC = yC - yA;

    int xStepAB = 0;
    int shadeStepAB = 0;
    if (yB != yA) {
        xStepAB = (dxAB << 16) / dyAB;
        shadeStepAB = ((shadeB - shadeA) << 16) / dyAB;
    }

    int xStepBC = 0;
    int shadeStepBC = 0;
    if (yC != yB) {
        xStepBC = ((xC - xB) << 16) / (yC - yB);
        shadeStepBC = ((shadeC - shadeB) << 16) / (yC - yB);
    }

    int xStepAC = 0;
    int shadeStepAC = 0;
    if (yC != yA) {
        xStepAC = ((xA - xC) << 16) / (yA - yC);
        shadeStepAC = ((shadeA - shadeC) << 16) / (yA - yC);
    }

    // this won't change any rendering, saves not wasting time "drawing" an invalid triangle
    int triangleArea = (dxAB * dyAC) - (dyAB * dxAC);
    if (triangleArea == 0) {
        return;
    }

    if (yA <= yB && yA <= yC) {
        if (yA < _Pix2D.bottom) {
            if (yB > _Pix2D.bottom) {
                yB = _Pix2D.bottom;
            }

            if (yC > _Pix2D.bottom) {
                yC = _Pix2D.bottom;
            }

            if (yB < yC) {
                xC = xA <<= 16;
                shadeC = shadeA <<= 16;
                if (yA < 0) {
                    xC -= xStepAC * yA;
                    xA -= xStepAB * yA;
                    shadeC -= shadeStepAC * yA;
                    shadeA -= shadeStepAB * yA;
                    yA = 0;
                }

                xB <<= 16;
                shadeB <<= 16;
                if (yB < 0) {
                    xB -= xStepBC * yB;
                    shadeB -= shadeStepBC * yB;
                    yB = 0;
                }

                int dy = yA - _Pix3D.center_y;
                u += uStepVertical * dy;
                v += vStepVertical * dy;
                w += wStepVertical * dy;

                if ((yA != yB && xStepAC < xStepAB) || (yA == yB && xStepAC > xStepBC)) {
                    yC -= yB;
                    yB -= yA;
                    yA = _Pix3D.line_offset[yA];

                    while (--yB >= 0) {
                        textureRaster(xC >> 16, xA >> 16, _Pix2D.pixels, yA, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeC >> 8, shadeA >> 8);
                        xC += xStepAC;
                        xA += xStepAB;
                        shadeC += shadeStepAC;
                        shadeA += shadeStepAB;
                        yA += _Pix2D.width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                    while (--yC >= 0) {
                        textureRaster(xC >> 16, xB >> 16, _Pix2D.pixels, yA, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeC >> 8, shadeB >> 8);
                        xC += xStepAC;
                        xB += xStepBC;
                        shadeC += shadeStepAC;
                        shadeB += shadeStepBC;
                        yA += _Pix2D.width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                } else {
                    yC -= yB;
                    yB -= yA;
                    yA = _Pix3D.line_offset[yA];

                    while (--yB >= 0) {
                        textureRaster(xA >> 16, xC >> 16, _Pix2D.pixels, yA, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeA >> 8, shadeC >> 8);
                        xC += xStepAC;
                        xA += xStepAB;
                        shadeC += shadeStepAC;
                        shadeA += shadeStepAB;
                        yA += _Pix2D.width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                    while (--yC >= 0) {
                        textureRaster(xB >> 16, xC >> 16, _Pix2D.pixels, yA, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeB >> 8, shadeC >> 8);
                        xC += xStepAC;
                        xB += xStepBC;
                        shadeC += shadeStepAC;
                        shadeB += shadeStepBC;
                        yA += _Pix2D.width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                }
            } else {
                xB = xA <<= 16;
                shadeB = shadeA <<= 16;
                if (yA < 0) {
                    xB -= xStepAC * yA;
                    xA -= xStepAB * yA;
                    shadeB -= shadeStepAC * yA;
                    shadeA -= shadeStepAB * yA;
                    yA = 0;
                }

                xC <<= 16;
                shadeC <<= 16;
                if (yC < 0) {
                    xC -= xStepBC * yC;
                    shadeC -= shadeStepBC * yC;
                    yC = 0;
                }

                int dy = yA - _Pix3D.center_y;
                u += uStepVertical * dy;
                v += vStepVertical * dy;
                w += wStepVertical * dy;

                if ((yA == yC || xStepAC >= xStepAB) && (yA != yC || xStepBC <= xStepAB)) {
                    yB -= yC;
                    yC -= yA;
                    yA = _Pix3D.line_offset[yA];

                    while (--yC >= 0) {
                        textureRaster(xA >> 16, xB >> 16, _Pix2D.pixels, yA, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeA >> 8, shadeB >> 8);
                        xB += xStepAC;
                        xA += xStepAB;
                        shadeB += shadeStepAC;
                        shadeA += shadeStepAB;
                        yA += _Pix2D.width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                    while (--yB >= 0) {
                        textureRaster(xA >> 16, xC >> 16, _Pix2D.pixels, yA, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeA >> 8, shadeC >> 8);
                        xC += xStepBC;
                        xA += xStepAB;
                        shadeC += shadeStepBC;
                        shadeA += shadeStepAB;
                        yA += _Pix2D.width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                } else {
                    yB -= yC;
                    yC -= yA;
                    yA = _Pix3D.line_offset[yA];

                    while (--yC >= 0) {
                        textureRaster(xB >> 16, xA >> 16, _Pix2D.pixels, yA, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeB >> 8, shadeA >> 8);
                        xB += xStepAC;
                        xA += xStepAB;
                        shadeB += shadeStepAC;
                        shadeA += shadeStepAB;
                        yA += _Pix2D.width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                    while (--yB >= 0) {
                        textureRaster(xC >> 16, xA >> 16, _Pix2D.pixels, yA, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeC >> 8, shadeA >> 8);
                        xC += xStepBC;
                        xA += xStepAB;
                        shadeC += shadeStepBC;
                        shadeA += shadeStepAB;
                        yA += _Pix2D.width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                }
            }
        }
    } else if (yB <= yC) {
        if (yB < _Pix2D.bottom) {
            if (yC > _Pix2D.bottom) {
                yC = _Pix2D.bottom;
            }

            if (yA > _Pix2D.bottom) {
                yA = _Pix2D.bottom;
            }

            if (yC < yA) {
                xA = xB <<= 16;
                shadeA = shadeB <<= 16;
                if (yB < 0) {
                    xA -= xStepAB * yB;
                    xB -= xStepBC * yB;
                    shadeA -= shadeStepAB * yB;
                    shadeB -= shadeStepBC * yB;
                    yB = 0;
                }

                xC <<= 16;
                shadeC <<= 16;
                if (yC < 0) {
                    xC -= xStepAC * yC;
                    shadeC -= shadeStepAC * yC;
                    yC = 0;
                }

                int dy = yB - _Pix3D.center_y;
                u += uStepVertical * dy;
                v += vStepVertical * dy;
                w += wStepVertical * dy;

                if ((yB != yC && xStepAB < xStepBC) || (yB == yC && xStepAB > xStepAC)) {
                    yA -= yC;
                    yC -= yB;
                    yB = _Pix3D.line_offset[yB];

                    while (--yC >= 0) {
                        textureRaster(xA >> 16, xB >> 16, _Pix2D.pixels, yB, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeA >> 8, shadeB >> 8);
                        xA += xStepAB;
                        xB += xStepBC;
                        shadeA += shadeStepAB;
                        shadeB += shadeStepBC;
                        yB += _Pix2D.width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                    while (--yA >= 0) {
                        textureRaster(xA >> 16, xC >> 16, _Pix2D.pixels, yB, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeA >> 8, shadeC >> 8);
                        xA += xStepAB;
                        xC += xStepAC;
                        shadeA += shadeStepAB;
                        shadeC += shadeStepAC;
                        yB += _Pix2D.width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                } else {
                    yA -= yC;
                    yC -= yB;
                    yB = _Pix3D.line_offset[yB];

                    while (--yC >= 0) {
                        textureRaster(xB >> 16, xA >> 16, _Pix2D.pixels, yB, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeB >> 8, shadeA >> 8);
                        xA += xStepAB;
                        xB += xStepBC;
                        shadeA += shadeStepAB;
                        shadeB += shadeStepBC;
                        yB += _Pix2D.width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                    while (--yA >= 0) {
                        textureRaster(xC >> 16, xA >> 16, _Pix2D.pixels, yB, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeC >> 8, shadeA >> 8);
                        xA += xStepAB;
                        xC += xStepAC;
                        shadeA += shadeStepAB;
                        shadeC += shadeStepAC;
                        yB += _Pix2D.width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                }
            } else {
                xC = xB <<= 16;
                shadeC = shadeB <<= 16;
                if (yB < 0) {
                    xC -= xStepAB * yB;
                    xB -= xStepBC * yB;
                    shadeC -= shadeStepAB * yB;
                    shadeB -= shadeStepBC * yB;
                    yB = 0;
                }

                xA <<= 16;
                shadeA <<= 16;
                if (yA < 0) {
                    xA -= xStepAC * yA;
                    shadeA -= shadeStepAC * yA;
                    yA = 0;
                }

                int dy = yB - _Pix3D.center_y;
                u += uStepVertical * dy;
                v += vStepVertical * dy;
                w += wStepVertical * dy;

                if (xStepAB < xStepBC) {
                    yC -= yA;
                    yA -= yB;
                    yB = _Pix3D.line_offset[yB];

                    while (--yA >= 0) {
                        textureRaster(xC >> 16, xB >> 16, _Pix2D.pixels, yB, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeC >> 8, shadeB >> 8);
                        xC += xStepAB;
                        xB += xStepBC;
                        shadeC += shadeStepAB;
                        shadeB += shadeStepBC;
                        yB += _Pix2D.width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                    while (--yC >= 0) {
                        textureRaster(xA >> 16, xB >> 16, _Pix2D.pixels, yB, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeA >> 8, shadeB >> 8);
                        xA += xStepAC;
                        xB += xStepBC;
                        shadeA += shadeStepAC;
                        shadeB += shadeStepBC;
                        yB += _Pix2D.width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                } else {
                    yC -= yA;
                    yA -= yB;
                    yB = _Pix3D.line_offset[yB];

                    while (--yA >= 0) {
                        textureRaster(xB >> 16, xC >> 16, _Pix2D.pixels, yB, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeB >> 8, shadeC >> 8);
                        xC += xStepAB;
                        xB += xStepBC;
                        shadeC += shadeStepAB;
                        shadeB += shadeStepBC;
                        yB += _Pix2D.width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                    while (--yC >= 0) {
                        textureRaster(xB >> 16, xA >> 16, _Pix2D.pixels, yB, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeB >> 8, shadeA >> 8);
                        xA += xStepAC;
                        xB += xStepBC;
                        shadeA += shadeStepAC;
                        shadeB += shadeStepBC;
                        yB += _Pix2D.width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                }
            }
        }
    } else if (yC < _Pix2D.bottom) {
        if (yA > _Pix2D.bottom) {
            yA = _Pix2D.bottom;
        }

        if (yB > _Pix2D.bottom) {
            yB = _Pix2D.bottom;
        }

        if (yA < yB) {
            xB = xC <<= 16;
            shadeB = shadeC <<= 16;
            if (yC < 0) {
                xB -= xStepBC * yC;
                xC -= xStepAC * yC;
                shadeB -= shadeStepBC * yC;
                shadeC -= shadeStepAC * yC;
                yC = 0;
            }

            xA <<= 16;
            shadeA <<= 16;
            if (yA < 0) {
                xA -= xStepAB * yA;
                shadeA -= shadeStepAB * yA;
                yA = 0;
            }

            int dy = yC - _Pix3D.center_y;
            u += uStepVertical * dy;
            v += vStepVertical * dy;
            w += wStepVertical * dy;

            if (xStepBC < xStepAC) {
                yB -= yA;
                yA -= yC;
                yC = _Pix3D.line_offset[yC];

                while (--yA >= 0) {
                    textureRaster(xB >> 16, xC >> 16, _Pix2D.pixels, yC, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeB >> 8, shadeC >> 8);
                    xB += xStepBC;
                    xC += xStepAC;
                    shadeB += shadeStepBC;
                    shadeC += shadeStepAC;
                    yC += _Pix2D.width;
                    u += uStepVertical;
                    v += vStepVertical;
                    w += wStepVertical;
                }
                while (--yB >= 0) {
                    textureRaster(xB >> 16, xA >> 16, _Pix2D.pixels, yC, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeB >> 8, shadeA >> 8);
                    xB += xStepBC;
                    xA += xStepAB;
                    shadeB += shadeStepBC;
                    shadeA += shadeStepAB;
                    yC += _Pix2D.width;
                    u += uStepVertical;
                    v += vStepVertical;
                    w += wStepVertical;
                }
            } else {
                yB -= yA;
                yA -= yC;
                yC = _Pix3D.line_offset[yC];

                while (--yA >= 0) {
                    textureRaster(xC >> 16, xB >> 16, _Pix2D.pixels, yC, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeC >> 8, shadeB >> 8);
                    xB += xStepBC;
                    xC += xStepAC;
                    shadeB += shadeStepBC;
                    shadeC += shadeStepAC;
                    yC += _Pix2D.width;
                    u += uStepVertical;
                    v += vStepVertical;
                    w += wStepVertical;
                }
                while (--yB >= 0) {
                    textureRaster(xA >> 16, xB >> 16, _Pix2D.pixels, yC, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeA >> 8, shadeB >> 8);
                    xB += xStepBC;
                    xA += xStepAB;
                    shadeB += shadeStepBC;
                    shadeA += shadeStepAB;
                    yC += _Pix2D.width;
                    u += uStepVertical;
                    v += vStepVertical;
                    w += wStepVertical;
                }
            }
        } else {
            xA = xC <<= 16;
            shadeA = shadeC <<= 16;
            if (yC < 0) {
                xA -= xStepBC * yC;
                xC -= xStepAC * yC;
                shadeA -= shadeStepBC * yC;
                shadeC -= shadeStepAC * yC;
                yC = 0;
            }

            xB <<= 16;
            shadeB <<= 16;
            if (yB < 0) {
                xB -= xStepAB * yB;
                shadeB -= shadeStepAB * yB;
                yB = 0;
            }

            int dy = yC - _Pix3D.center_y;
            u += uStepVertical * dy;
            v += vStepVertical * dy;
            w += wStepVertical * dy;

            if (xStepBC < xStepAC) {
                yA -= yB;
                yB -= yC;
                yC = _Pix3D.line_offset[yC];

                while (--yB >= 0) {
                    textureRaster(xA >> 16, xC >> 16, _Pix2D.pixels, yC, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeA >> 8, shadeC >> 8);
                    xA += xStepBC;
                    xC += xStepAC;
                    shadeA += shadeStepBC;
                    shadeC += shadeStepAC;
                    yC += _Pix2D.width;
                    u += uStepVertical;
                    v += vStepVertical;
                    w += wStepVertical;
                }
                while (--yA >= 0) {
                    textureRaster(xB >> 16, xC >> 16, _Pix2D.pixels, yC, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeB >> 8, shadeC >> 8);
                    xB += xStepAB;
                    xC += xStepAC;
                    shadeB += shadeStepAB;
                    shadeC += shadeStepAC;
                    yC += _Pix2D.width;
                    u += uStepVertical;
                    v += vStepVertical;
                    w += wStepVertical;
                }
            } else {
                yA -= yB;
                yB -= yC;
                yC = _Pix3D.line_offset[yC];

                while (--yB >= 0) {
                    textureRaster(xC >> 16, xA >> 16, _Pix2D.pixels, yC, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeC >> 8, shadeA >> 8);
                    xA += xStepBC;
                    xC += xStepAC;
                    shadeA += shadeStepBC;
                    shadeC += shadeStepAC;
                    yC += _Pix2D.width;
                    u += uStepVertical;
                    v += vStepVertical;
                    w += wStepVertical;
                }
                while (--yA >= 0) {
                    textureRaster(xC >> 16, xB >> 16, _Pix2D.pixels, yC, texels, 0, 0, u, v, w, uStride, vStride, wStride, shadeC >> 8, shadeB >> 8);
                    xB += xStepAB;
                    xC += xStepAC;
                    shadeB += shadeStepAB;
                    shadeC += shadeStepAC;
                    yC += _Pix2D.width;
                    u += uStepVertical;
                    v += vStepVertical;
                    w += wStepVertical;
                }
            }
        }
    }
}

void textureRaster(int xA, int xB, int *dst, int offset, int *texels, int curU, int curV, int u, int v, int w, int uStride, int vStride, int wStride, int shadeA, int shadeB) {
    if (xA >= xB) {
        return;
    }

    int shadeStrides;
    int strides;
    if (_Pix3D.clipX) {
        shadeStrides = (shadeB - shadeA) / (xB - xA);

        if (xB > _Pix2D.bound_x) {
            xB = _Pix2D.bound_x;
        }

        if (xA < 0) {
            shadeA -= xA * shadeStrides;
            xA = 0;
        }

        if (xA >= xB) {
            return;
        }

        strides = (xB - xA) >> 3;
        shadeStrides <<= 12;
        shadeA <<= 9;
    } else {
        if (xB - xA > 7) {
            strides = (xB - xA) >> 3;
            shadeStrides = (shadeB - shadeA) * _Pix3D.reciprical15[strides] >> 6;
        } else {
            strides = 0;
            shadeStrides = 0;
        }

        shadeA <<= 9;
    }

    offset += xA;

    if (_Pix3D.lowMemory) {
        int nextU = 0;
        int nextV = 0;
        int dx = xA - _Pix3D.center_x;

        u = u + (uStride >> 3) * dx;
        v = v + (vStride >> 3) * dx;
        w = w + (wStride >> 3) * dx;

        int curW = w >> 12;
        if (curW != 0) {
            curU = u / curW;
            curV = v / curW;
            if (curU < 0) {
                curU = 0;
            } else if (curU > 0xfc0) {
                curU = 0xfc0;
            }
        }

        u = u + uStride;
        v = v + vStride;
        w = w + wStride;

        curW = w >> 12;
        if (curW != 0) {
            nextU = u / curW;
            nextV = v / curW;
            if (nextU < 0x7) {
                nextU = 0x7;
            } else if (nextU > 0xfc0) {
                nextU = 0xfc0;
            }
        }

        int stepU = (nextU - curU) >> 3;
        int stepV = (nextV - curV) >> 3;
        curU += (shadeA >> 3) & 0xc0000;
        int shadeShift = shadeA >> 23;

        if (_Pix3D.opaque) {
            while (strides-- > 0) {
                dst[offset++] = (uint32_t)texels[(curV & 0xfc0) + (curU >> 6)] >> shadeShift;
                curU += stepU;
                curV += stepV;

                dst[offset++] = (uint32_t)texels[(curV & 0xfc0) + (curU >> 6)] >> shadeShift;
                curU += stepU;
                curV += stepV;

                dst[offset++] = (uint32_t)texels[(curV & 0xfc0) + (curU >> 6)] >> shadeShift;
                curU += stepU;
                curV += stepV;

                dst[offset++] = (uint32_t)texels[(curV & 0xfc0) + (curU >> 6)] >> shadeShift;
                curU += stepU;
                curV += stepV;

                dst[offset++] = (uint32_t)texels[(curV & 0xfc0) + (curU >> 6)] >> shadeShift;
                curU += stepU;
                curV += stepV;

                dst[offset++] = (uint32_t)texels[(curV & 0xfc0) + (curU >> 6)] >> shadeShift;
                curU += stepU;
                curV += stepV;

                dst[offset++] = (uint32_t)texels[(curV & 0xfc0) + (curU >> 6)] >> shadeShift;
                curU += stepU;
                curV += stepV;

                dst[offset++] = (uint32_t)texels[(curV & 0xfc0) + (curU >> 6)] >> shadeShift;
                curU = nextU;
                curV = nextV;

                u += uStride;
                v += vStride;
                w += wStride;

                curW = w >> 12;
                if (curW != 0) {
                    nextU = u / curW;
                    nextV = v / curW;
                    if (nextU < 0x7) {
                        nextU = 0x7;
                    } else if (nextU > 0xfc0) {
                        nextU = 0xfc0;
                    }
                }

                stepU = (nextU - curU) >> 3;
                stepV = (nextV - curV) >> 3;
                shadeA += shadeStrides;
                curU += (shadeA >> 3) & 0xc0000;
                shadeShift = shadeA >> 23;
            }

            strides = xB - xA & 0x7;
            while (strides-- > 0) {
                dst[offset++] = (uint32_t)texels[(curV & 0xfc0) + (curU >> 6)] >> shadeShift;
                curU += stepU;
                curV += stepV;
            }
        } else {
            while (strides-- > 0) {
                int rgb;
                if ((rgb = (uint32_t)texels[(curV & 0xfc0) + (curU >> 6)] >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = (uint32_t)texels[(curV & 0xfc0) + (curU >> 6)] >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = (uint32_t)texels[(curV & 0xfc0) + (curU >> 6)] >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = (uint32_t)texels[(curV & 0xfc0) + (curU >> 6)] >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = (uint32_t)texels[(curV & 0xfc0) + (curU >> 6)] >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = (uint32_t)texels[(curV & 0xfc0) + (curU >> 6)] >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = (uint32_t)texels[(curV & 0xfc0) + (curU >> 6)] >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = (uint32_t)texels[(curV & 0xfc0) + (curU >> 6)] >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU = nextU;
                curV = nextV;

                u += uStride;
                v += vStride;
                w += wStride;

                curW = w >> 12;
                if (curW != 0) {
                    nextU = u / curW;
                    nextV = v / curW;
                    if (nextU < 7) {
                        nextU = 7;
                    } else if (nextU > 0xfc0) {
                        nextU = 0xfc0;
                    }
                }

                stepU = (nextU - curU) >> 3;
                stepV = (nextV - curV) >> 3;
                shadeA += shadeStrides;
                curU += (shadeA >> 3) & 0xc0000;
                shadeShift = shadeA >> 23;
            }

            strides = (xB - xA) & 0x7;
            while (strides-- > 0) {
                int rgb;
                if ((rgb = (uint32_t)texels[(curV & 0xfc0) + (curU >> 6)] >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }

                offset++;
                curU += stepU;
                curV += stepV;
            }
        }
    } else {
        int nextU = 0;
        int nextV = 0;
        int dx = xA - _Pix3D.center_x;

        u = u + (uStride >> 3) * dx;
        v = v + (vStride >> 3) * dx;
        w = w + (wStride >> 3) * dx;

        int curW = w >> 14;
        if (curW != 0) {
            curU = u / curW;
            curV = v / curW;
            if (curU < 0) {
                curU = 0;
            } else if (curU > 0x3f80) {
                curU = 0x3f80;
            }
        }

        u = u + uStride;
        v = v + vStride;
        w = w + wStride;

        curW = w >> 14;
        if (curW != 0) {
            nextU = u / curW;
            nextV = v / curW;
            if (nextU < 0x7) {
                nextU = 0x7;
            } else if (nextU > 0x3f80) {
                nextU = 0x3f80;
            }
        }

        int stepU = (nextU - curU) >> 3;
        int stepV = (nextV - curV) >> 3;
        curU += shadeA & 0x600000;
        int shadeShift = shadeA >> 23;

        if (_Pix3D.opaque) {
            while (strides-- > 0) {
                dst[offset++] = (uint32_t)texels[(curV & 0x3F80) + (curU >> 7)] >> shadeShift;
                curU += stepU;
                curV += stepV;

                dst[offset++] = (uint32_t)texels[(curV & 0x3F80) + (curU >> 7)] >> shadeShift;
                curU += stepU;
                curV += stepV;

                dst[offset++] = (uint32_t)texels[(curV & 0x3F80) + (curU >> 7)] >> shadeShift;
                curU += stepU;
                curV += stepV;

                dst[offset++] = (uint32_t)texels[(curV & 0x3F80) + (curU >> 7)] >> shadeShift;
                curU += stepU;
                curV += stepV;

                dst[offset++] = (uint32_t)texels[(curV & 0x3F80) + (curU >> 7)] >> shadeShift;
                curU += stepU;
                curV += stepV;

                dst[offset++] = (uint32_t)texels[(curV & 0x3F80) + (curU >> 7)] >> shadeShift;
                curU += stepU;
                curV += stepV;

                dst[offset++] = (uint32_t)texels[(curV & 0x3F80) + (curU >> 7)] >> shadeShift;
                curU += stepU;
                curV += stepV;

                dst[offset++] = (uint32_t)texels[(curV & 0x3F80) + (curU >> 7)] >> shadeShift;
                curU = nextU;
                curV = nextV;

                u += uStride;
                v += vStride;
                w += wStride;

                curW = w >> 14;
                if (curW != 0) {
                    nextU = u / curW;
                    nextV = v / curW;
                    if (nextU < 0x7) {
                        nextU = 0x7;
                    } else if (nextU > 0x3f80) {
                        nextU = 0x3f80;
                    }
                }

                stepU = (nextU - curU) >> 3;
                stepV = (nextV - curV) >> 3;
                shadeA += shadeStrides;
                curU += shadeA & 0x600000;
                shadeShift = shadeA >> 23;
            }

            strides = xB - xA & 0x7;
            while (strides-- > 0) {
                dst[offset++] = (uint32_t)texels[(curV & 0x3F80) + (curU >> 7)] >> shadeShift;
                curU += stepU;
                curV += stepV;
            }
        } else {
            while (strides-- > 0) {
                int rgb;
                if ((rgb = (uint32_t)texels[(curV & 0x3F80) + (curU >> 7)] >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = (uint32_t)texels[(curV & 0x3F80) + (curU >> 7)] >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = (uint32_t)texels[(curV & 0x3F80) + (curU >> 7)] >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = (uint32_t)texels[(curV & 0x3F80) + (curU >> 7)] >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = (uint32_t)texels[(curV & 0x3F80) + (curU >> 7)] >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = (uint32_t)texels[(curV & 0x3F80) + (curU >> 7)] >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = (uint32_t)texels[(curV & 0x3F80) + (curU >> 7)] >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = (uint32_t)texels[(curV & 0x3F80) + (curU >> 7)] >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU = nextU;
                curV = nextV;

                u += uStride;
                v += vStride;
                w += wStride;

                curW = w >> 14;
                if (curW != 0) {
                    nextU = u / curW;
                    nextV = v / curW;
                    if (nextU < 0x7) {
                        nextU = 0x7;
                    } else if (nextU > 0x3f80) {
                        nextU = 0x3f80;
                    }
                }

                stepU = (nextU - curU) >> 3;
                stepV = (nextV - curV) >> 3;
                shadeA += shadeStrides;
                curU += shadeA & 0x600000;
                shadeShift = shadeA >> 23;
            }

            strides = xB - xA & 0x7;
            while (strides-- > 0) {
                int rgb;
                if ((rgb = (uint32_t)texels[(curV & 0x3F80) + (curU >> 7)] >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }

                offset++;
                curU += stepU;
                curV += stepV;
            }
        }
    }
}
