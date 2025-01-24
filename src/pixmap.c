#include <stdlib.h>
#include <string.h>

#include "pix2d.h"
#include "pixmap.h"

PixMap *pixmap_new(GameShell *shell, int width, int height) {
    PixMap *pixmap = calloc(1, sizeof(PixMap));
    pixmap->shell = shell;
    pixmap->width = width;
    pixmap->height = height;
    pixmap->image = platform_create_surface(width, height, 0);
    pixmap->pixels = calloc(pixmap->width * pixmap->height, sizeof(int));
    pixmap_bind(pixmap);
    return pixmap;
}

void pixmap_free(PixMap *pixmap) {
    free(pixmap->pixels);
    platform_free_surface(pixmap->image);
    free(pixmap);
}

void pixmap_clear(PixMap *pixmap) {
    memset(pixmap->pixels, 0, pixmap->width * pixmap->height * sizeof(int));
}

void pixmap_bind(PixMap *pixmap) {
    pix2d_bind(pixmap->width, pixmap->height, pixmap->pixels);
}

void pixmap_draw(PixMap *pixmap, int x, int y) {
    set_pixels(pixmap, x, y);
}
