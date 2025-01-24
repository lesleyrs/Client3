#include <stdlib.h>

#include "tileunderlay.h"

TileUnderlay *tileunderlay_new(int southwestColor, int southeastColor, int northeastColor, int northwestColor, int textureId, int rgb, bool flat) {
    TileUnderlay *underlay = calloc(1, sizeof(TileUnderlay));
    // underlay->flat = true;

    underlay->southwestColor = southwestColor;
    underlay->southeastColor = southeastColor;
    underlay->northeastColor = northeastColor;
    underlay->northwestColor = northwestColor;
    underlay->textureId = textureId;
    underlay->rgb = rgb;
    underlay->flat = flat;
    return underlay;
}
