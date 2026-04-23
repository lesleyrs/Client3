#include <stdlib.h>
#include <string.h>

#include "gl11.h"
#include "custom.h"
#include "defines.h"
#include "pix2d.h"
#include "pix3d.h"

extern Pix3D _Pix3D;
extern Pix2D _Pix2D;
extern Custom _Custom;

#ifdef GL11
#ifdef __vita__
int vertxoff = (SCREEN_FB_WIDTH - SCREEN_WIDTH) / 2 + 8;
#else
int vertxoff = 8;
#endif

Vertex verts[100000]; // NOTE make sure it's high enough
int vertcount;

static bool use_opengl11;
#endif

// NOTE: everything gl-related not in this file is also behind GL11 define

void gl_start_frame(void) {
#ifdef GL11
    use_opengl11 = _Custom.use_opengl11;
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
#endif
}

void gl_start_drawscene(void) {
#ifdef GL11
    glEnable(GL_SCISSOR_TEST);
#if 1
    // leave a black line on right side of viewport (see pix2d.c)
    glScissor(vertxoff, SCREEN_FB_HEIGHT - 11 - _Pix2D.height, _Pix2D.width - 1, _Pix2D.height);
#else
    // use this for non-black clear color values
    glScissor(vertxoff, SCREEN_FB_HEIGHT - 11 - _Pix2D.height, _Pix2D.width, _Pix2D.height);
    glBegin(GL_LINES);
    glColor4ub(0, 0, 0, 0xff);
    glVertex2i(vertxoff + _Pix2D.width, 11);
    glVertex2i(vertxoff + _Pix2D.width, 11 + 334);
    glEnd();
#endif
#endif
}

#ifdef GL_NO_IMMEDIATE
uint32_t texture_atlas;
#endif

void gl_end_drawscene(void) {
#ifdef GL11
#ifdef GL_NO_IMMEDIATE
    if (vertcount > 0) {
        glEnable(GL_TEXTURE_2D);

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        glBindTexture(GL_TEXTURE_2D, texture_atlas);

        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), &verts[0].r);
        glVertexPointer(2, GL_FLOAT, sizeof(Vertex), &verts[0].x);
        glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), &verts[0].u);

        glDrawArrays(GL_TRIANGLES, 0, vertcount);

        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);

        glDisable(GL_TEXTURE_2D);

        // rs2_log("verts %d\n", vertcount);
        vertcount = 0;
    }
#endif
    glDisable(GL_SCISSOR_TEST);
    // scene is rendered with gl, the rest must be in software so pixmaps don't draw over interface models
    _Custom.use_opengl11 = false;
#endif
}

void gl_end_frame(void) {
#ifdef GL11
    _Custom.use_opengl11 = use_opengl11;
#endif
}

void gl_set_brightness(void) {
#ifdef GL11
#ifdef GL_NO_IMMEDIATE
    glDeleteTextures(1, &texture_atlas);

    int texture_size = 128; // _Pix3D.textures[id]->width/height
    if (_Pix3D.lowMemory) {
        texture_size = 64;
    }
    int pixel_count = texture_size * texture_size;

    glGenTextures(1, &texture_atlas);
    glBindTexture(GL_TEXTURE_2D, texture_atlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP); // this is clamped manually in pix3d now since all textures are combined

    uint32_t *pixels = malloc(ATLAS_TEXTURE_COUNT * pixel_count * sizeof(uint32_t));
    memset(pixels, 0xff, pixel_count * sizeof(uint32_t)); // white texture at idx 0

    for (int id = 0; id < 50; id++) { // _Pix3D.textureCount
        int *texels = pix3d_get_texels(id);

        if (!texels) {
            continue;
        }

        int atlas_width = texture_size * ATLAS_TEXTURE_COUNT;
        int atlas_xoff = texture_size * (id + 1);

        for (int y = 0; y < texture_size; y++) {
            for (int x = 0; x < texture_size; x++) {
                int rgb = texels[y * texture_size + x];

                if (rgb != 0) {
                    rgb |= 0xff000000;
                }
                pixels[y * atlas_width + atlas_xoff + x] = rgb;
            }
        }
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_size * ATLAS_TEXTURE_COUNT, texture_size, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels);
    free(pixels);
#else
    for (int id = 0; id < 50; id++) { // _Pix3D.textureCount
        Pix8 *texture = _Pix3D.textures[id];
        int *texels = pix3d_get_texels(id);

        if (!texels) {
            continue;
        }

        int n = texture->width * texture->height;
        uint32_t *pixels = malloc(n * sizeof(uint32_t));

        for (int i = 0; i < n; i++) {
            int rgb = texels[i];

            if (rgb != 0) {
                rgb |= 0xff000000;
            }
            pixels[i] = rgb;
        }
        glGenTextures(1, &texture->gl_texture);
        glBindTexture(GL_TEXTURE_2D, texture->gl_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->width, texture->height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);

        free(pixels);
    }
#endif
#endif
}
