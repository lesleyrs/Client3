#include <stdlib.h>

#include "pix2d.h"
#include "pixmap.h"
#include "gl11.h"

#ifdef GL11
int pixmap_xoff = 0;
int pixmap_yoff = 0;
#endif

PixMap *pixmap_new(int width, int height) {
    PixMap *pixmap = calloc(1, sizeof(PixMap));
    pixmap->width = width;
    pixmap->height = height;
    pixmap->pixels = calloc(pixmap->width * pixmap->height, sizeof(int));
    pixmap->image = platform_create_surface(pixmap->pixels, width, height, false);
    pixmap_bind(pixmap);
#ifdef GL11
    glGenTextures(1, &pixmap->gl_texture);
    glBindTexture(GL_TEXTURE_2D, pixmap->gl_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#endif
    return pixmap;
}

void pixmap_free(PixMap *pixmap) {
    platform_free_surface(pixmap->image);
    free(pixmap->pixels);
    free(pixmap);
}

void pixmap_bind(PixMap *pixmap) {
    pix2d_bind(pixmap->width, pixmap->height, pixmap->pixels);
}

void pixmap_draw(PixMap *pixmap, int x, int y) {
#ifdef GL11
    // TODO cache these?
    glBindTexture(GL_TEXTURE_2D, pixmap->gl_texture);
    int n = pixmap->width * pixmap->height;
    uint32_t *pixels = malloc(n * 4);

    for (int i = 0; i < n; i++) {
        uint32_t rgb = pixmap->pixels[i];
        if (rgb == 0xffffffff) {
            rgb = 0;
        } else {
            rgb |= 0xff000000;
        }
        pixels[i] = rgb;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixmap->width, pixmap->height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels);

    glEnable(GL_TEXTURE_2D);

    glColor4ub(0xff, 0xff, 0xff, 0xff);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x, y);
    glTexCoord2f(1, 0); glVertex2f(x + pixmap->width, y);
    glTexCoord2f(1, 1); glVertex2f(x + pixmap->width, y + pixmap->height);
    glTexCoord2f(0, 1); glVertex2f(x, y + pixmap->height);
    glEnd();

    glDisable(GL_TEXTURE_2D);

    free(pixels);
#else
    platform_blit_surface(pixmap->image, x, y);
#endif
}
