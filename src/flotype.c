#include <stdlib.h>

#include "flotype.h"
#include "jagfile.h"
#include "packet.h"
#include "platform.h"

FloTypeData _FloType = {0};

static void flotype_decode(FloType *flo, Packet *dat);

static FloType *flotype_new(void) {
    FloType *flo = calloc(1, sizeof(FloType));
    flo->texture = -1;
    flo->occlude = true;
    return flo;
}

void flotype_free_global(void) {
    for (int i = 0; i < _FloType.count; i++) {
        free(_FloType.instances[i]->name);
        free(_FloType.instances[i]);
    }
    free(_FloType.instances);
}

void flotype_unpack(Jagfile *config) {
    Packet *dat = jagfile_to_packet(config, "flo.dat");
    _FloType.count = g2(dat);

    if (!_FloType.instances) {
        _FloType.instances = calloc(_FloType.count, sizeof(FloType *));
    }

    for (int id = 0; id < _FloType.count; id++) {
        if (!_FloType.instances[id]) {
            _FloType.instances[id] = flotype_new();
        }

        flotype_decode(_FloType.instances[id], dat);
    }

    packet_free(dat);
}

static void flotype_decode(FloType *flo, Packet *dat) {
    while (true) {
        int code = g1(dat);
        if (code == 0) {
            return;
        }

        if (code == 1) {
            flo->rgb = g3(dat);
            flotype_set_color(flo, flo->rgb);
        } else if (code == 2) {
            flo->texture = g1(dat);
        } else if (code == 3) {
            flo->overlay = true;
        } else if (code == 5) {
            flo->occlude = false;
        } else if (code == 6) {
            flo->name = gjstr(dat);
        } else {
            rs2_error("Error unrecognised flo config code: %d\n", code);
        }
    }
}

void flotype_set_color(FloType *flo, int rgb) {
    double red = (double)(rgb >> 16 & 0xff) / 256.0;
    double green = (double)(rgb >> 8 & 0xff) / 256.0;
    double blue = (double)(rgb & 0xff) / 256.0;

    double min = red;
    if (green < red) {
        min = green;
    }
    if (blue < min) {
        min = blue;
    }

    double max = red;
    if (green > red) {
        max = green;
    }
    if (blue > max) {
        max = blue;
    }

    double h = 0.0;
    double s = 0.0;
    double l = (min + max) / 2.0;

    if (min != max) {
        if (l < 0.5) {
            s = (max - min) / (max + min);
        }
        if (l >= 0.5) {
            s = (max - min) / (2.0 - max - min);
        }

        if (red == max) {
            h = (green - blue) / (max - min);
        } else if (green == max) {
            h = (blue - red) / (max - min) + 2.0;
        } else if (blue == max) {
            h = (red - green) / (max - min) + 4.0;
        }
    }

    h /= 6.0;

    flo->hue = (int)(h * 256.0);
    flo->saturation = (int)(s * 256.0);
    flo->lightness = (int)(l * 256.0);

    if (flo->saturation < 0) {
        flo->saturation = 0;
    } else if (flo->saturation > 255) {
        flo->saturation = 255;
    }

    if (flo->lightness < 0) {
        flo->lightness = 0;
    } else if (flo->lightness > 255) {
        flo->lightness = 255;
    }

    if (l > 0.5) {
        flo->luminance = (int)((1.0 - l) * s * 512.0);
    } else {
        flo->luminance = (int)(l * s * 512.0);
    }

    if (flo->luminance < 1) {
        flo->luminance = 1;
    }

    flo->chroma = (int)(h * (double)flo->luminance);

    int hue = flo->hue + (int)(jrand() * 16.0) - 8;
    if (hue < 0) {
        hue = 0;
    } else if (hue > 255) {
        hue = 255;
    }

    int saturation = flo->saturation + (int)(jrand() * 48.0) - 24;
    if (saturation < 0) {
        saturation = 0;
    } else if (saturation > 255) {
        saturation = 255;
    }

    int lightness = flo->lightness + (int)(jrand() * 48.0) - 24;
    if (lightness < 0) {
        lightness = 0;
    } else if (lightness > 255) {
        lightness = 255;
    }

    flo->hsl = hsl24to16(hue, saturation, lightness);
}

int hsl24to16(int hue, int saturation, int lightness) {
    if (lightness > 179) {
        saturation /= 2;
    }

    if (lightness > 192) {
        saturation /= 2;
    }

    if (lightness > 217) {
        saturation /= 2;
    }

    if (lightness > 243) {
        saturation /= 2;
    }

    return (hue / 4 << 10) + (saturation / 32 << 7) + lightness / 2;
}

int adjustLightness(int hsl, int scalar) {
    if (hsl == -2) {
        return 12345678;
    }

    if (hsl == -1) {
        if (scalar < 0) {
            scalar = 0;
        } else if (scalar > 127) {
            scalar = 127;
        }
        return 127 - scalar;
    } else {
        scalar = scalar * (hsl & 0x7f) / 128;
        if (scalar < 2) {
            scalar = 2;
        } else if (scalar > 126) {
            scalar = 126;
        }
        return (hsl & 0xff80) + scalar;
    }
}

int mulHSL(int hsl, int lightness) {
    if (hsl == -1) {
        return 12345678;
    }

    lightness = lightness * (hsl & 0x7f) / 128;
    if (lightness < 2) {
        lightness = 2;
    } else if (lightness > 126) {
        lightness = 126;
    }

    return (hsl & 0xff80) + lightness;
}
