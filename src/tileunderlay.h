#pragma once

#include <stdbool.h>

typedef struct {
    int southwestColor;
    int southeastColor;
    int northeastColor;
    int northwestColor;
    int textureId;
    bool flat;
    int rgb;
} TileUnderlay;

TileUnderlay *tileunderlay_new(int southwestColor, int southeastColor, int northeastColor, int northwestColor, int textureId, int rgb, bool flat);
