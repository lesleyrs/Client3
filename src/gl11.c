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

#ifdef GL_NO_IMMEDIATE
Vertex verts[100000]; // NOTE make sure it's high enough
int vertcount;
#endif

static uint32_t texture_atlas;

static bool use_opengl11;
static bool use_anisotropic;

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
#ifndef GL_NO_IMMEDIATE
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture_atlas);
    glBegin(GL_TRIANGLES);
#endif

#endif
}

void gl_end_drawscene(void) {
#ifdef GL11

#ifndef GL_NO_IMMEDIATE
    glEnd();
    glDisable(GL_TEXTURE_2D);
#endif
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
    int texture_size = 128; // _Pix3D.textures[id]->width/height
    if (_Pix3D.lowMemory) {
        texture_size = 64;
    }
    int pixel_count = texture_size * texture_size;

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

    glBindTexture(GL_TEXTURE_2D, texture_atlas);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_size * ATLAS_TEXTURE_COUNT, texture_size, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels);
    free(pixels);

    if (use_anisotropic) {
#define GL_GENERATE_MIPMAP                0x8191
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP); // this is clamped manually in pix3d now since all textures are combined

    if (use_anisotropic) {
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
        float amount = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &amount);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, amount);
    }
#endif
}

#ifdef GL11
// https://www.opengl.org/archives/resources/features/OGLextensions/
static int isExtensionSupported(const char *extension) {
  const GLubyte *extensions = NULL;
  const GLubyte *start;
  GLubyte *where, *terminator;

  /* Extension names should not have spaces. */
  where = (GLubyte *) strchr(extension, ' ');
  if (where || *extension == '\0')
    return 0;
  extensions = glGetString(GL_EXTENSIONS);
  /* It takes a bit of care to be fool-proof about parsing the
     OpenGL extensions string. Don't be fooled by sub-strings,
     etc. */
  start = extensions;
  for (;;) {
    where = (GLubyte *) strstr((const char *) start, extension);
    if (!where)
      break;
    terminator = where + strlen(extension);
    if (where == start || *(where - 1) == ' ')
      if (*terminator == ' ' || *terminator == '\0')
        return 1;
    start = terminator;
  }
  return 0;
}

static bool gl_load_extension(const char *ext) {
    if (isExtensionSupported(ext)) {
        rs2_log("%s available\n", ext);
        return true;
    }
    return false;
}
#endif

void gl_load(void) {
#ifdef GL11
    rs2_log("OpenGL: %s, %s, %s\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));

    const char *extensions = (const char*)glGetString(GL_EXTENSIONS);
    if (!extensions) {
        return;
    }

    if (!gl_load_extension("GL_EXT_bgra")) {
        rs2_log("GL_EXT_bgra extension not found\n");
        exit(1);
    }

    if (gl_load_extension("GL_SGIS_generate_mipmap") && gl_load_extension("GL_EXT_texture_filter_anisotropic")) {
        use_anisotropic = true;
    }

    rs2_log("\n");

    glGenTextures(1, &texture_atlas);
#endif
}
