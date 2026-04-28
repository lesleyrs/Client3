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
    pixmap->gl_pixels = calloc(pixmap->width * pixmap->height, sizeof(int));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixmap->width, pixmap->height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixmap->gl_pixels);
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

void pixmap_draw(PixMap *pixmap, int x, int y) {
#ifdef GL11

#ifdef __vita__
    #include "defines.h"
    x += (SCREEN_FB_WIDTH - SCREEN_WIDTH) / 2;
#endif

    glBindTexture(GL_TEXTURE_2D, pixmap->gl_texture);
    if (pixmap->dirty) {
        // rs2_log("dirty pixmap id %d width %d height %d\n", pixmap->gl_texture, pixmap->width, pixmap->height);
        pixmap->dirty = false;

        for (int i = 0; i < pixmap->width * pixmap->height; i++) {
            uint32_t rgb = pixmap->pixels[i];
            if (rgb == 0xffffffff) {
                rgb = 0;
            } else {
                rgb |= 0xff000000;
            }
            pixmap->gl_pixels[i] = rgb;
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pixmap->width, pixmap->height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixmap->gl_pixels);
    }

    glEnable(GL_TEXTURE_2D);

    glColor4ub(0xff, 0xff, 0xff, 0xff);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2i(x, y);
    glTexCoord2f(1, 0); glVertex2i(x + pixmap->width, y);
    glTexCoord2f(1, 1); glVertex2i(x + pixmap->width, y + pixmap->height);
    glTexCoord2f(0, 1); glVertex2i(x, y + pixmap->height);
    glEnd();

    glDisable(GL_TEXTURE_2D);
#else
    platform_blit_surface(pixmap->image, x, y);
#endif
}
