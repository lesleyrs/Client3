#include <math.h>
#include <stdio.h>
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
extern SceneData _World3D;

#ifdef GL11

Vertex verts[300000];
int vertcount;
uint32_t indices[100000];
int elementcount;

uint32_t texture_atlas;

static bool use_opengl11;
static bool use_anisotropic;
#endif

// NOTE: everything gl-related not in this file is also behind GL11 define OR _Custom.use_opengl11

void gl_start_frame(void) {
#ifdef GL11
    use_opengl11 = _Custom.use_opengl11;
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif
}

void gl_end_frame(void) {
#ifdef GL11
    _Custom.use_opengl11 = use_opengl11;
#endif
}

void gl_start_drawscene(void) {
#ifdef GL11
#endif
}

void gl_end_drawscene(Client *c) {
#ifdef GL11
    if (elementcount > 0) {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        int offsetX = 0;
        int offsetY = 0;

        int left = ((offsetX - _Pix3D.center_x) << 9) / DEFAULT_ZOOM;
        int right = ((offsetX + c->area_viewport->width - _Pix3D.center_x) << 9) / DEFAULT_ZOOM;
        int top = ((offsetY - _Pix3D.center_y) << 9) / DEFAULT_ZOOM;
        int bottom = ((offsetY + c->area_viewport->height - _Pix3D.center_y) << 9) / DEFAULT_ZOOM;

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glFrustum(left * FRUSTUM_SCALE, right * FRUSTUM_SCALE, -bottom * FRUSTUM_SCALE, -top * FRUSTUM_SCALE, Z_NEAR, Z_FAR);
        glRotatef(PI_DEGREES, 1, 0, 0);

        if (c->cameraPitch != 0) {
            glRotatef(c->cameraPitch * RS_TO_DEGREES, 1, 0, 0);
        }
        if (c->cameraYaw != 0) {
            glRotatef(c->cameraYaw * RS_TO_DEGREES, 0, 1, 0);
        }
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(-_World3D.eyeX, -_World3D.eyeY, -_World3D.eyeZ);

        // leave a black line on right side of viewport (see pix2d.c)
        int viewport_xoff = 8;
    #ifdef __vita__
        viewport_xoff += SCREEN_CENTER_XOFF;
    #endif
        glViewport(viewport_xoff, SCREEN_FB_HEIGHT - 11 - _Pix2D.height, _Pix2D.width - 1, _Pix2D.height);
        glBindTexture(GL_TEXTURE_2D, texture_atlas);

        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), &verts[0].r);
        glVertexPointer(3, GL_FLOAT, sizeof(Vertex), &verts[0].x);
        glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), &verts[0].u);

        char buf[MAX_STR];
        uint64_t last = rs2_now();
        glDrawElements(GL_TRIANGLES, elementcount, GL_UNSIGNED_INT, indices);
        if (_Custom.show_performance) {
            sprintf(buf, "glDrawElements: %lu ms", rs2_now() - last);
            drawStringRight(c->font_plain11, 507, 226, buf, YELLOW, true);

            sprintf(buf, "verts/indices: %d %d", vertcount, elementcount);
            drawStringRight(c->font_plain11, 507, 239, buf, YELLOW, true);
        }

        vertcount = 0;
        elementcount = 0;

        glViewport(0, 0, SCREEN_FB_WIDTH, SCREEN_FB_HEIGHT);

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
    }

    // scene is rendered with gl, the rest must be in software so pixmaps don't draw over interface models
    _Custom.use_opengl11 = false;
#endif
}

void gl_set_brightness(void) {
#ifdef GL11
    int texture_size = 128; // _Pix3D.textures[id]->width/height
    if (_Pix3D.lowMemory) {
        texture_size = 64;
    }

    int atlas_width = ATLAS_TEXTURE_COUNT * texture_size;

    uint32_t *pixels = malloc(atlas_width * texture_size * sizeof(uint32_t));
    for (int y = 0; y < texture_size; y++) {
        for (int x = 0; x < texture_size; x++) {
            pixels[y * atlas_width + x] = 0xffffffff; // white texture at idx 0
        }
    }

    for (int id = 0; id < 50; id++) { // _Pix3D.textureCount
        int *texels = pix3d_get_texels(id);

        if (!texels) {
            continue;
        }

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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas_width, texture_size, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels);
    free(pixels);

    if (use_anisotropic) {
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
        float amount = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &amount);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, amount);

#define GL_GENERATE_MIPMAP                0x8191
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP); // this is clamped manually in pix3d now since all textures are combined
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

// source:
// https://rune-server.org/threads/texture-mapping-conversions-pmn-uv-pmn.701381/
// https://rune-server.org/threads/better-texture-mapping-conversions.706373/

UV pmn_to_uv(int xA, int yA, int zA, int xB, int yB, int zB, int xC, int yC, int zC, int xP, int yP, int zP, int xM, int yM, int zM, int xN, int yN, int zN) {
    int mX = xM - xP;
    int mY = yM - yP;
    int mZ = zM - zP;

    int nX = xN - xP;
    int nY = yN - yP;
    int nZ = zN - zP;

    int aX = xA - xP;
    int aY = yA - yP;
    int aZ = zA - zP;

    int bX = xB - xP;
    int bY = yB - yP;
    int bZ = zB - zP;

    int cX = xC - xP;
    int cY = yC - yP;
    int cZ = zC - zP;

    int MxNx = mY * nZ - mZ * nY;
    int MxNy = mZ * nX - mX * nZ;
    int MxNz = mX * nY - mY * nX;

    int uX = nY * MxNz - nZ * MxNy;
    int uY = nZ * MxNx - nX * MxNz;
    int uZ = nX * MxNy - nY * MxNx;

    float mU = 1.0f / (uX * mX + uY * mY + uZ * mZ);
    float uA = (uX * aX + uY * aY + uZ * aZ) * mU;
    float uB = (uX * bX + uY * bY + uZ * bZ) * mU;
    float uC = (uX * cX + uY * cY + uZ * cZ) * mU;

    int vX = mY * MxNz - mZ * MxNy;
    int vY = mZ * MxNx - mX * MxNz;
    int vZ = mX * MxNy - mY * MxNx;

    float mV = 1.0f / (vX * nX + vY * nY + vZ * nZ);
    float vA = (vX * aX + vY * aY + vZ * aZ) * mV;
    float vB = (vX * bX + vY * bY + vZ * bZ) * mV;
    float vC = (vX * cX + vY * cY + vZ * cZ) * mV;

    return (UV){uA, uB, uC, vA, vB, vC};
}
#endif

void gl_load(void) {
#ifdef GL11
    rs2_log("OpenGL: %s, %s, %s\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));

    int size = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);

    int lowmem_atlas_size = ATLAS_TEXTURE_COUNT * 64;
    int highmem_atlas_size = ATLAS_TEXTURE_COUNT * 128;

    if (size < lowmem_atlas_size) {
        rs2_error("GL_MAX_TEXTURE_SIZE: %d/%d won't fit in texture atlas\n", size, lowmem_atlas_size);
        exit(1);
    }

    if (!_Pix3D.lowMemory && size < highmem_atlas_size) {
        _Pix3D.lowMemory = true;
        rs2_log("GL_MAX_TEXTURE_SIZE: %d/%d, using low memory textures\n", size, highmem_atlas_size);
    }

    const char *extensions = (const char*)glGetString(GL_EXTENSIONS);
    if (!extensions) {
        rs2_error("No extensions found\n");
        exit(1);
    }

    if (!gl_load_extension("GL_EXT_bgra")) {
        rs2_error("GL_EXT_bgra extension not found\n");
        exit(1);
    }

    if (!gl_load_extension("GL_ARB_texture_non_power_of_two") && !gl_load_extension("GL_OES_texture_npot")) {
        rs2_error("NPOT textures are not supported\n");
        exit(1);
    }

    if (gl_load_extension("GL_SGIS_generate_mipmap") && gl_load_extension("GL_EXT_texture_filter_anisotropic")) {
        // NOTE: makes transparent textures like fishing spots/fountain water darker and hard to see
        // use_anisotropic = true;
    }

    rs2_log("\n");

    glGenTextures(1, &texture_atlas);

    glEnable(GL_TEXTURE_2D);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_CULL_FACE);
    // glEnable(GL_DEPTH_TEST);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, SCREEN_FB_WIDTH, SCREEN_FB_HEIGHT, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
#endif
}

void glGouraudTriangle(int xA, int xB, int xC, int yA, int yB, int yC, int zA, int zB, int zC, int colorA, int colorB, int colorC, int alpha) {
    (void)xA, (void)xB, (void)xC, (void)yA, (void)yB, (void)yC, (void)zA, (void)zB, (void)zC, (void)colorA, (void)colorB, (void)colorC, (void)alpha;
#ifdef GL11
    Vertex v0 = {(_Pix3D.palette[colorA] >> 16) & 0xff, (_Pix3D.palette[colorA] >> 8) & 0xff, _Pix3D.palette[colorA] & 0xff, alpha, xA, yA, zA, 0, 0};
    Vertex v1 = {(_Pix3D.palette[colorB] >> 16) & 0xff, (_Pix3D.palette[colorB] >> 8) & 0xff, _Pix3D.palette[colorB] & 0xff, alpha, xB, yB, zB, 0, 0};
    Vertex v2 = {(_Pix3D.palette[colorC] >> 16) & 0xff, (_Pix3D.palette[colorC] >> 8) & 0xff, _Pix3D.palette[colorC] & 0xff, alpha, xC, yC, zC, 0, 0};

    verts[vertcount++] = v0;
    verts[vertcount++] = v1;
    verts[vertcount++] = v2;

    // glTexCoord2f(0, 0);
    // glColor4ub((_Pix3D.palette[colorA] >> 16) & 0xff, (_Pix3D.palette[colorA] >> 8) & 0xff, _Pix3D.palette[colorA] & 0xff, alpha);
    // glVertex3i(xA, yA, zA);
    // glColor4ub((_Pix3D.palette[colorB] >> 16) & 0xff, (_Pix3D.palette[colorB] >> 8) & 0xff, _Pix3D.palette[colorB] & 0xff, alpha);
    // glVertex3i(xB, yB, zB);
    // glColor4ub((_Pix3D.palette[colorC] >> 16) & 0xff, (_Pix3D.palette[colorC] >> 8) & 0xff, _Pix3D.palette[colorC] & 0xff, alpha);
    // glVertex3i(xC, yC, zC);
#endif
}

void glTextureTriangle(int xA, int xB, int xC, int yA, int yB, int yC, int zA, int zB, int zC, int shadeA, int shadeB, int shadeC, UV uv, int texture) {
    (void)xA, (void)xB, (void)xC, (void)yA, (void)yB, (void)yC, (void)zA, (void)zB, (void)zC, (void)shadeA, (void)shadeB, (void)shadeC, (void)uv, (void)texture;
#ifdef GL11
    extern ClientData _Client;
    if (!_Client.lowmem) {
        // scrolling textures from updateTextures
        if (texture == 17 || texture == 24) {
            float time = rs2_now() / 1000.0f;
            float texture_anim_unit = 1.0f / 128.0f;
            float offset = time / 0.02f * -2.0f * texture_anim_unit;
            offset = fmodf(offset, 1.0f);
            uv.vA += offset;
            uv.vB += offset;
            uv.vC += offset;
        }
    }

    int shadeShiftA = 255 - (shadeA << 1);
    int shadeShiftB = 255 - (shadeB << 1);
    int shadeShiftC = 255 - (shadeC << 1);

    // manual clamp since opengl will no longer do it for us
    uv.uA = clamp01(uv.uA);
    uv.uB = clamp01(uv.uB);
    uv.uC = clamp01(uv.uC);

    int texture_idx = 1 + texture; // after default white texture
    uv.uA = (texture_idx + uv.uA) / ATLAS_TEXTURE_COUNT;
    uv.uB = (texture_idx + uv.uB) / ATLAS_TEXTURE_COUNT;
    uv.uC = (texture_idx + uv.uC) / ATLAS_TEXTURE_COUNT;

    Vertex v0 = {shadeShiftA, shadeShiftA, shadeShiftA, 0xff, xA, yA, zA, uv.uA, uv.vA};
    Vertex v1 = {shadeShiftB, shadeShiftB, shadeShiftB, 0xff, xB, yB, zB, uv.uB, uv.vB};
    Vertex v2 = {shadeShiftC, shadeShiftC, shadeShiftC, 0xff, xC, yC, zC, uv.uC, uv.vC};

    verts[vertcount++] = v0;
    verts[vertcount++] = v1;
    verts[vertcount++] = v2;

    // glColor4ub(shadeShiftA, shadeShiftA, shadeShiftA, 0xff);
    // glTexCoord2f(uv.uA, uv.vA);
    // glVertex3i(xA, yA, zA);

    // glColor4ub(shadeShiftB, shadeShiftB, shadeShiftB, 0xff);
    // glTexCoord2f(uv.uB, uv.vB);
    // glVertex3i(xB, yB, zB);

    // glColor4ub(shadeShiftC, shadeShiftC, shadeShiftC, 0xff);
    // glTexCoord2f(uv.uC, uv.vC);
    // glVertex3i(xC, yC, zC);
#endif
}

void gl_start_model(Model *model, int sceneX, int sceneY, int sceneZ, int yaw) {
    (void)model, (void)sceneX, (void)sceneY, (void)sceneZ, (void)yaw;
#ifdef GL11
    if (!_Custom.use_opengl11) {
        return;
    }
    int *triangleColorsA = model->face_color_a;
    int *triangleColorsB = model->face_color_b;
    int *triangleColorsC = model->face_color_c;

    if (!triangleColorsA || !triangleColorsB || !triangleColorsC) {
        return;
    }

    int *verticesX = model->vertices_x;
    int *verticesY = model->vertices_y;
    int *verticesZ = model->vertices_z;

    int *triangleA = model->face_indices_a;
    int *triangleB = model->face_indices_b;
    int *triangleC = model->face_indices_c;

    int *triangleColors = model->face_colors;

    int *triangleAlphas = model->face_alphas;

    int *triangleInfos = model->face_infos;

    int *textureMappingP = model->textured_p_coordinate;
    int *textureMappingM = model->textured_m_coordinate;
    int *textureMappingN = model->textured_n_coordinate;

    int triangleCount = model->face_count;

    // glPushMatrix();
    // extern SceneData _World3D;
    // glTranslatef(sceneX + _World3D.eyeX, sceneY + _World3D.eyeY, sceneZ + _World3D.eyeZ);
    // glRotatef(yaw * RS_TO_DEGREES, 0, 1, 0);

    for (int t = 0; t < triangleCount; t++) {
        int a = triangleA[t];
        int b = triangleB[t];
        int c = triangleC[t];

        int xa = verticesX[a];
        int ya = verticesY[a];
        int za = verticesZ[a];

        int xb = verticesX[b];
        int yb = verticesY[b];
        int zb = verticesZ[b];

        int xc = verticesX[c];
        int yc = verticesY[c];
        int zc = verticesZ[c];

        int colorA = triangleColorsA[t];
        int colorB = triangleColorsB[t];
        int colorC = triangleColorsC[t];

        int alpha = 0xff;
        if (triangleAlphas) {
            alpha = 0xff - triangleAlphas[t];
        }

        int info = 0;
        if (triangleInfos) {
            info = triangleInfos[t];
        }

        int type = info & 0x3;

        // Flat shading
        if (type == 1 || type == 3) {
            colorC = colorB = colorA;
        }

        int textureId = -1;

        // TODO test perf
        // rotateY
        int sin = _Pix3D.sin_table[(2048 - yaw) & 0x7ff];
        int cos = _Pix3D.cos_table[(2048 - yaw) & 0x7ff];

        int tmp = xa;
        xa = (tmp * cos - za * sin) >> 16;
        za = (tmp * sin + za * cos) >> 16;

        tmp = xb;
        xb = (tmp * cos - zb * sin) >> 16;
        zb = (tmp * sin + zb * cos) >> 16;

        tmp = xc;
        xc = (tmp * cos - zc * sin) >> 16;
        zc = (tmp * sin + zc * cos) >> 16;

        // translate
        xa += sceneX + _World3D.eyeX;
        xb += sceneX + _World3D.eyeX;
        xc += sceneX + _World3D.eyeX;
        ya += sceneY + _World3D.eyeY;
        yb += sceneY + _World3D.eyeY;
        yc += sceneY + _World3D.eyeY;
        za += sceneZ + _World3D.eyeZ;
        zb += sceneZ + _World3D.eyeZ;
        zc += sceneZ + _World3D.eyeZ;

        // Textured
        if ((type == 2 || type == 3) && triangleColors) {
            textureId = triangleColors[t];

            int texCoord = info >> 2;
            int p = textureMappingP[texCoord];
            int m = textureMappingM[texCoord];
            int n = textureMappingN[texCoord];

            UV uv = pmn_to_uv(verticesX[a], verticesY[a], verticesZ[a], verticesX[b], verticesY[b], verticesZ[b], verticesX[c], verticesY[c], verticesZ[c], verticesX[p], verticesY[p], verticesZ[p], verticesX[m], verticesY[m], verticesZ[m], verticesX[n], verticesY[n], verticesZ[n]);

            glTextureTriangle(xa, xb, xc, ya, yb, yc, za, zb, zc, colorA, colorB, colorC, uv, textureId);
        } else {
            glGouraudTriangle(xa, xb, xc, ya, yb, yc, za, zb, zc, colorA, colorB, colorC, alpha);
        }
    }

    // glPopMatrix();
#endif
}
