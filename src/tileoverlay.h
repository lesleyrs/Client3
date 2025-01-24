#pragma once

#include <stdbool.h>

typedef struct {
    int *vertexX;
    int *vertexY;
    int *vertexZ;
    int *triangleColorA;
    int *triangleColorB;
    int *triangleColorC;
    int *triangleVertexA;
    int *triangleVertexB;
    int *triangleVertexC;
    int *triangleTextureIds;
    bool flat;
    int shape;
    int rotation;
    int backgroundRgb;
    int foregroundRgb;

    int vertexCount;
    int triangleCount;
} TileOverlay;

typedef struct {
    int tmpScreenX[6];
    int tmpScreenY[6];
    int tmpViewspaceX[6];
    int tmpViewspaceY[6];
    int tmpViewspaceZ[6];
} TileOverlayData;

TileOverlay *tileoverlay_new(int tileX, int shape, int southeastColor2, int southeastY, int northeastColor1, int rotation, int southwestColor1, int northwestY, int foregroundRgb, int southwestColor2, int textureId, int northwestColor2, int backgroundRgb, int northeastY, int northeastColor2, int northwestColor1, int southwestY, int tileZ, int southeastColor1);
void tileoverlay_free(TileOverlay *overlay);
