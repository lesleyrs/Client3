#include <stdlib.h>

#include "pix2d.h"
#include "pixmap.h"
#include "gl11.h"

PixMap *pixmap_new(int width, int height) {
    PixMap *pixmap = calloc(1, sizeof(PixMap));
    pixmap->width = width;
    pixmap->height = height;
    pixmap->pixels = calloc(pixmap->width * pixmap->height, sizeof(int));
    pixmap_bind(pixmap);
#ifndef GL11
    pixmap->image = platform_create_surface(pixmap->pixels, width, height, false);
#else
    glGenTextures(1, &pixmap->gl_texture);
    glBindTexture(GL_TEXTURE_2D, pixmap->gl_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int pot_width = next_po2(width);
    int pot_height = next_po2(height);
    pixmap->u = (float)pixmap->width / pot_width;
    pixmap->v = (float)pixmap->height / pot_height;
    pixmap->gl_pixels = calloc(pot_width * pot_height, sizeof(int));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pot_width, pot_height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixmap->gl_pixels);
#endif
    return pixmap;
}

void pixmap_free(PixMap *pixmap) {
#ifdef GL11
    free(pixmap->gl_pixels);
    glDeleteTextures(1, &pixmap->gl_texture);
#else
    platform_free_surface(pixmap->image);
#endif
    free(pixmap->pixels);
    free(pixmap);
}

void pixmap_bind(PixMap *pixmap) {
#ifdef GL11
    pixmap->dirty = true;
#endif
    pix2d_bind(pixmap->width, pixmap->height, pixmap->pixels);
}

#ifdef GL11
typedef struct {
    uint8_t r, g, b;
    uint16_t x, y;
} Point;

#define MAX_IDX_NUMBER 0xC000 // for vitaGL compat
Point pixels[MAX_IDX_NUMBER]; // area_viewport pixels
int pixcount = 0;

// tcc has no builtins, and a prefetch is only ever a hint
#if defined(__GNUC__) && !defined(__TINYC__)
#define PIXMAP_PREFETCH(addr) __builtin_prefetch(addr)
#else
#define PIXMAP_PREFETCH(addr) ((void)0)
#endif

static inline void push_point(uint32_t rgb, int sx, int sy) {
    Point p = {(rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff, sx, sy};
    pixels[pixcount++] = p;
}
#endif

void pixmap_draw(PixMap *pixmap, int x, int y) {
#ifdef GL11
#ifdef __vita__
    x += SCREEN_CENTER_XOFF;
#endif

    extern bool use_opengl11; // use global bool instead of _Custom.use_opengl11 (force disabled after scene render)
    // this path is for few pixels, if use_opengl11 was disabled area_viewport contains the entire scene as well
    if (use_opengl11 && pixmap->width == 512) { // area_viewport pixmap is usually dirty while often drawing NOTHING
        const uint32_t *src = (const uint32_t *)pixmap->pixels;
        const int width = pixmap->width;
        const int height = pixmap->height;

        pixcount = 0; // reset pixcount b4 loop as it's drawn on screen later
        for (int py = 0; py < height; py++) {
            if (pixcount >= MAX_IDX_NUMBER - width) { // might not reach MAX_IDX_NUMBER but prefer outer loop check
                goto draw_texture;
            }

#if 1
            const uint32_t *row = src + py * width;
            int px = 0;

            // process 4 pixels at a time
            for (; px + 3 < width; px += 4) {
                PIXMAP_PREFETCH(row + px + 64); // this hint measurably improves access on vita

                if ((row[px] & row[px + 1] & row[px + 2] & row[px + 3]) == 0xffffffff) {
                    continue;
                }

                // optimization will unroll this
                for (int i = 0; i < 4; i++) {
                    if (row[px + i] != 0xffffffff) {
                        push_point(row[px + i], x + px + i, y + py);
                    }
                }
            }

            // process remainder (if width is not / 4)
            for (; px < width; px++) {
                if (row[px] != 0xffffffff) {
                    push_point(row[px], x + px, y + py);
                }
            }
#else
            for (int px = 0; px < >width; px++) {
                uint32_t rgb = src[py * width + px];
                if (rgb != 0xffffffff) {
                    Point p = {(rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff, x + px, y + py};
                    pixels[pixcount++] = p;
                }
            }
#endif
        }

        if (pixcount > 0) {
            glDisable(GL_TEXTURE_2D);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);

            glColorPointer(3, GL_UNSIGNED_BYTE, sizeof(Point), &pixels[0].r);
            glVertexPointer(2, GL_SHORT, sizeof(Point), &pixels[0].x);
            glDrawArrays(GL_POINTS, 0, pixcount);

            glEnable(GL_TEXTURE_2D);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        }
        return;
    }

    draw_texture:
    glBindTexture(GL_TEXTURE_2D, pixmap->gl_texture);
    if (pixmap->dirty) {
        // rs2_log("dirty pixmap id %d width %d height %d\n", pixmap->gl_texture, pixmap->width, pixmap->height);
        pixmap->dirty = false;

#if 1
        const uint32_t *restrict src = (const uint32_t *)pixmap->pixels;
        uint32_t *restrict dst = (uint32_t *)pixmap->gl_pixels;
        const int count = pixmap->width * pixmap->height;

        // with -O3 this becomes SIMD (vector instructions)
        for (int i = 0; i < count; i++) {
            uint32_t rgb = src[i];
            uint32_t drawn = (uint32_t)0 - (uint32_t)(rgb != 0xffffffff);
            dst[i] = (rgb | 0xff000000) & drawn;
        }
#else
        for (int i = 0; i < pixmap->width * pixmap->height; i++) {
            uint32_t rgb = pixmap->pixels[i];
            if (rgb == 0xffffffff) {
                rgb = 0;
            } else {
                rgb |= 0xff000000;
            }
            pixmap->gl_pixels[i] = rgb;
        }
#endif
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pixmap->width, pixmap->height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixmap->gl_pixels);
    }

    glColor4ub(0xff, 0xff, 0xff, 0xff);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2i(x, y);
    glTexCoord2f(0, pixmap->v); glVertex2i(x, y + pixmap->height);
    glTexCoord2f(pixmap->u, pixmap->v); glVertex2i(x + pixmap->width, y + pixmap->height);
    glTexCoord2f(pixmap->u, 0); glVertex2i(x + pixmap->width, y);
    glEnd();
#else
    platform_blit_surface(pixmap->image, x, y);
#endif
}
