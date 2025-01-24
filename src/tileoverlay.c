#include <stdlib.h>

#include "tileoverlay.h"

TileOverlayData _TileOverlay = {0};

int *SHAPE_POINTS[] = {
    (int[]){1, 3, 5, 7},
    (int[]){1, 3, 5, 7},         // PLAIN_SHAPE
    (int[]){1, 3, 5, 7},         // DIAGONAL_SHAPE
    (int[]){1, 3, 5, 7, 6},      // LEFT_SEMI_DIAGONAL_SMALL_SHAPE
    (int[]){1, 3, 5, 7, 6},      // RIGHT_SEMI_DIAGONAL_SMALL_SHAPE
    (int[]){1, 3, 5, 7, 6},      // LEFT_SEMI_DIAGONAL_BIG_SHAPE
    (int[]){1, 3, 5, 7, 6},      // RIGHT_SEMI_DIAGONAL_BIG_SHAPE
    (int[]){1, 3, 5, 7, 2, 6},   // HALF_SQUARE_SHAPE
    (int[]){1, 3, 5, 7, 2, 8},   // CORNER_SMALL_SHAPE
    (int[]){1, 3, 5, 7, 2, 8},   // CORNER_BIG_SHAPE
    (int[]){1, 3, 5, 7, 11, 12}, // FAN_SMALL_SHAPE
    (int[]){1, 3, 5, 7, 11, 12}, // FAN_BIG_SHAPE
    (int[]){1, 3, 5, 7, 13, 14}  // TRAPEZIUM_SHAPE
};

int SHAPE_POINTS_SIZES[] = {
    4,
    4, // PLAIN_SHAPE
    4, // DIAGONAL_SHAPE
    5, // LEFT_SEMI_DIAGONAL_SMALL_SHAPE
    5, // RIGHT_SEMI_DIAGONAL_SMALL_SHAPE
    5, // LEFT_SEMI_DIAGONAL_BIG_SHAPE
    5, // RIGHT_SEMI_DIAGONAL_BIG_SHAPE
    6, // HALF_SQUARE_SHAPE
    6, // CORNER_SMALL_SHAPE
    6, // CORNER_BIG_SHAPE
    6, // FAN_SMALL_SHAPE
    6, // FAN_BIG_SHAPE
    6  // TRAPEZIUM_SHAPE
};

int *SHAPE_PATHS[] = {
    (int[]){0, 1, 2, 3, 0, 0, 1, 3},
    (int[]){1, 1, 2, 3, 1, 0, 1, 3},                                                 // PLAIN_SHAPE
    (int[]){0, 1, 2, 3, 1, 0, 1, 3},                                                 // DIAGONAL_SHAPE
    (int[]){0, 0, 1, 2, 0, 0, 2, 4, 1, 0, 4, 3},                                     // LEFT_SEMI_DIAGONAL_SMALL_SHAPE
    (int[]){0, 0, 1, 4, 0, 0, 4, 3, 1, 1, 2, 4},                                     // RIGHT_SEMI_DIAGONAL_SMALL_SHAPE
    (int[]){0, 0, 4, 3, 1, 0, 1, 2, 1, 0, 2, 4},                                     // LEFT_SEMI_DIAGONAL_BIG_SHAPE
    (int[]){0, 1, 2, 4, 1, 0, 1, 4, 1, 0, 4, 3},                                     // RIGHT_SEMI_DIAGONAL_BIG_SHAPE
    (int[]){0, 4, 1, 2, 0, 4, 2, 5, 1, 0, 4, 5, 1, 0, 5, 3},                         // HALF_SQUARE_SHAPE
    (int[]){0, 4, 1, 2, 0, 4, 2, 3, 0, 4, 3, 5, 1, 0, 4, 5},                         // CORNER_SMALL_SHAPE
    (int[]){0, 0, 4, 5, 1, 4, 1, 2, 1, 4, 2, 3, 1, 4, 3, 5},                         // CORNER_BIG_SHAPE
    (int[]){0, 0, 1, 5, 0, 1, 4, 5, 0, 1, 2, 4, 1, 0, 5, 3, 1, 5, 4, 3, 1, 4, 2, 3}, // FAN_SMALL_SHAPE
    (int[]){1, 0, 1, 5, 1, 1, 4, 5, 1, 1, 2, 4, 0, 0, 5, 3, 0, 5, 4, 3, 0, 4, 2, 3}, // FAN_BIG_SHAPE
    (int[]){1, 0, 5, 4, 1, 0, 1, 5, 0, 0, 4, 3, 0, 4, 5, 3, 0, 5, 2, 3, 0, 1, 2, 5}  // TRAPEZIUM_SHAPE
};

int SHAPE_PATHS_SIZES[] = {
    8,
    8,  // PLAIN_SHAPE
    8,  // DIAGONAL_SHAPE
    12, // LEFT_SEMI_DIAGONAL_SMALL_SHAPE
    12, // RIGHT_SEMI_DIAGONAL_SMALL_SHAPE
    12, // LEFT_SEMI_DIAGONAL_BIG_SHAPE
    12, // RIGHT_SEMI_DIAGONAL_BIG_SHAPE
    16, // HALF_SQUARE_SHAPE
    16, // CORNER_SMALL_SHAPE
    16, // CORNER_BIG_SHAPE
    24, // FAN_SMALL_SHAPE
    24, // FAN_BIG_SHAPE
    24  // TRAPEZIUM_SHAPE
};

void tileoverlay_free(TileOverlay *overlay) {
    free(overlay->vertexX);
    free(overlay->vertexY);
    free(overlay->vertexZ);
    free(overlay->triangleColorA);
    free(overlay->triangleColorB);
    free(overlay->triangleColorC);
    free(overlay->triangleVertexA);
    free(overlay->triangleVertexB);
    free(overlay->triangleVertexC);
    free(overlay->triangleTextureIds);
    free(overlay);
}

TileOverlay *tileoverlay_new(int tileX, int shape, int southeastColor2, int southeastY, int northeastColor1, int rotation, int southwestColor1, int northwestY, int foregroundRgb, int southwestColor2, int textureId, int northwestColor2, int backgroundRgb, int northeastY, int northeastColor2, int northwestColor1, int southwestY, int tileZ, int southeastColor1) {
    TileOverlay *overlay = calloc(1, sizeof(TileOverlay));
    overlay->flat = true;

    if (southwestY != southeastY || southwestY != northeastY || southwestY != northwestY) {
        overlay->flat = false;
    }

    overlay->shape = shape;
    overlay->rotation = rotation;
    overlay->backgroundRgb = backgroundRgb;
    overlay->foregroundRgb = foregroundRgb;

    int ONE = 128;
    int HALF = ONE / 2;
    int QUARTER = ONE / 4;
    int THREE_QUARTER = ONE * 3 / 4;

    int *points = SHAPE_POINTS[shape];
    overlay->vertexCount = SHAPE_POINTS_SIZES[shape];
    overlay->vertexX = calloc(overlay->vertexCount, sizeof(int));
    overlay->vertexY = calloc(overlay->vertexCount, sizeof(int));
    overlay->vertexZ = calloc(overlay->vertexCount, sizeof(int));
    int *primaryColors = calloc(overlay->vertexCount, sizeof(int));
    int *secondaryColors = calloc(overlay->vertexCount, sizeof(int));

    int sceneX = tileX * ONE;
    int sceneZ = tileZ * ONE;

    for (int v = 0; v < overlay->vertexCount; v++) {
        int type = points[v];

        if ((type & 0x1) == 0 && type <= 8) {
            type = (type - rotation - rotation - 1 & 0x7) + 1;
        }

        if (type > 8 && type <= 12) {
            type = (type - rotation - 9 & 0x3) + 9;
        }

        if (type > 12 && type <= 16) {
            type = (type - rotation - 13 & 0x3) + 13;
        }

        int x;
        int z;
        int y;
        int color1;
        int color2;

        if (type == 1) {
            x = sceneX;
            z = sceneZ;
            y = southwestY;
            color1 = southwestColor1;
            color2 = southwestColor2;
        } else if (type == 2) {
            x = sceneX + HALF;
            z = sceneZ;
            y = (southwestY + southeastY) >> 1;
            color1 = (southwestColor1 + southeastColor1) >> 1;
            color2 = (southwestColor2 + southeastColor2) >> 1;
        } else if (type == 3) {
            x = sceneX + ONE;
            z = sceneZ;
            y = southeastY;
            color1 = southeastColor1;
            color2 = southeastColor2;
        } else if (type == 4) {
            x = sceneX + ONE;
            z = sceneZ + HALF;
            y = (southeastY + northeastY) >> 1;
            color1 = (southeastColor1 + northeastColor1) >> 1;
            color2 = (southeastColor2 + northeastColor2) >> 1;
        } else if (type == 5) {
            x = sceneX + ONE;
            z = sceneZ + ONE;
            y = northeastY;
            color1 = northeastColor1;
            color2 = northeastColor2;
        } else if (type == 6) {
            x = sceneX + HALF;
            z = sceneZ + ONE;
            y = (northeastY + northwestY) >> 1;
            color1 = (northeastColor1 + northwestColor1) >> 1;
            color2 = (northeastColor2 + northwestColor2) >> 1;
        } else if (type == 7) {
            x = sceneX;
            z = sceneZ + ONE;
            y = northwestY;
            color1 = northwestColor1;
            color2 = northwestColor2;
        } else if (type == 8) {
            x = sceneX;
            z = sceneZ + HALF;
            y = (northwestY + southwestY) >> 1;
            color1 = (northwestColor1 + southwestColor1) >> 1;
            color2 = (northwestColor2 + southwestColor2) >> 1;
        } else if (type == 9) {
            x = sceneX + HALF;
            z = sceneZ + QUARTER;
            y = (southwestY + southeastY) >> 1;
            color1 = (southwestColor1 + southeastColor1) >> 1;
            color2 = (southwestColor2 + southeastColor2) >> 1;
        } else if (type == 10) {
            x = sceneX + THREE_QUARTER;
            z = sceneZ + HALF;
            y = (southeastY + northeastY) >> 1;
            color1 = (southeastColor1 + northeastColor1) >> 1;
            color2 = (southeastColor2 + northeastColor2) >> 1;
        } else if (type == 11) {
            x = sceneX + HALF;
            z = sceneZ + THREE_QUARTER;
            y = (northeastY + northwestY) >> 1;
            color1 = (northeastColor1 + northwestColor1) >> 1;
            color2 = (northeastColor2 + northwestColor2) >> 1;
        } else if (type == 12) {
            x = sceneX + QUARTER;
            z = sceneZ + HALF;
            y = (northwestY + southwestY) >> 1;
            color1 = (northwestColor1 + southwestColor1) >> 1;
            color2 = (northwestColor2 + southwestColor2) >> 1;
        } else if (type == 13) {
            x = sceneX + QUARTER;
            z = sceneZ + QUARTER;
            y = southwestY;
            color1 = southwestColor1;
            color2 = southwestColor2;
        } else if (type == 14) {
            x = sceneX + THREE_QUARTER;
            z = sceneZ + QUARTER;
            y = southeastY;
            color1 = southeastColor1;
            color2 = southeastColor2;
        } else if (type == 15) {
            x = sceneX + THREE_QUARTER;
            z = sceneZ + THREE_QUARTER;
            y = northeastY;
            color1 = northeastColor1;
            color2 = northeastColor2;
        } else {
            x = sceneX + QUARTER;
            z = sceneZ + THREE_QUARTER;
            y = northwestY;
            color1 = northwestColor1;
            color2 = northwestColor2;
        }

        overlay->vertexX[v] = x;
        overlay->vertexY[v] = y;
        overlay->vertexZ[v] = z;
        primaryColors[v] = color1;
        secondaryColors[v] = color2;
    }

    int *paths = SHAPE_PATHS[shape];
    overlay->triangleCount = SHAPE_PATHS_SIZES[shape] / 4;
    overlay->triangleVertexA = calloc(overlay->triangleCount, sizeof(int));
    overlay->triangleVertexB = calloc(overlay->triangleCount, sizeof(int));
    overlay->triangleVertexC = calloc(overlay->triangleCount, sizeof(int));
    overlay->triangleColorA = calloc(overlay->triangleCount, sizeof(int));
    overlay->triangleColorB = calloc(overlay->triangleCount, sizeof(int));
    overlay->triangleColorC = calloc(overlay->triangleCount, sizeof(int));

    if (textureId != -1) {
        overlay->triangleTextureIds = calloc(overlay->triangleCount, sizeof(int));
    }

    int index = 0;
    for (int t = 0; t < overlay->triangleCount; t++) {
        int color = paths[index];
        int a = paths[index + 1];
        int b = paths[index + 2];
        int c = paths[index + 3];
        index += 4;

        if (a < 4) {
            a = a - rotation & 0x3;
        }

        if (b < 4) {
            b = b - rotation & 0x3;
        }

        if (c < 4) {
            c = c - rotation & 0x3;
        }

        overlay->triangleVertexA[t] = a;
        overlay->triangleVertexB[t] = b;
        overlay->triangleVertexC[t] = c;

        if (color == 0) {
            overlay->triangleColorA[t] = primaryColors[a];
            overlay->triangleColorB[t] = primaryColors[b];
            overlay->triangleColorC[t] = primaryColors[c];

            if (overlay->triangleTextureIds) {
                overlay->triangleTextureIds[t] = -1;
            }
        } else {
            overlay->triangleColorA[t] = secondaryColors[a];
            overlay->triangleColorB[t] = secondaryColors[b];
            overlay->triangleColorC[t] = secondaryColors[c];

            if (overlay->triangleTextureIds) {
                overlay->triangleTextureIds[t] = textureId;
            }
        }
    }
    free(primaryColors);
    free(secondaryColors);
    return overlay;
}
