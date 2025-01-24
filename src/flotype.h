#pragma once

#include "jagfile.h"

// name taken from rs3
typedef struct {
    int rgb;
    int texture;  // = -1;
    bool overlay; // = false;
    bool occlude; // = true;
    char *name;
    int hue;
    int saturation;
    int lightness;
    int chroma;
    int luminance;
    int hsl;
} FloType;

typedef struct {
    int count;
    FloType **instances;
} FloTypeData;

void flotype_unpack(Jagfile *config);
void flotype_free_global(void);
void flotype_set_color(FloType *flo, int rgb);
int hsl24to16(int hue, int saturation, int lightness);
int adjustLightness(int hsl, int scalar);
int mulHSL(int hsl, int lightness);
