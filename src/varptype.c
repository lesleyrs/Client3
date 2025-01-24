#include <stdlib.h>

#include "jagfile.h"
#include "packet.h"
#include "platform.h"
#include "varptype.h"

VarpTypeData _VarpType = {0};

static void varptype_decode(VarpType *varp, int id, Packet *dat);

static VarpType *varptype_new(void) {
    VarpType *varp = calloc(1, sizeof(VarpType));
    varp->code4 = true;
    varp->code6 = false;
    varp->code8 = false;
    return varp;
}

void varptype_free_global(void) {
    for (int i = 0; i < _VarpType.count; i++) {
        free(_VarpType.instances[i]);
    }
    free(_VarpType.instances);
    free(_VarpType.code3);
}

void varptype_unpack(Jagfile *config) {
    Packet *dat = jagfile_to_packet(config, "varp.dat");
    _VarpType.code3Count = 0;
    _VarpType.count = g2(dat);

    if (!_VarpType.instances) {
        _VarpType.instances = calloc(_VarpType.count, sizeof(VarpType *));
    }

    if (!_VarpType.code3) {
        _VarpType.code3 = calloc(_VarpType.count, sizeof(int));
    }

    for (int id = 0; id < _VarpType.count; id++) {
        if (!_VarpType.instances[id]) {
            _VarpType.instances[id] = varptype_new();
        }

        varptype_decode(_VarpType.instances[id], id, dat);
    }

    packet_free(dat);
}

static void varptype_decode(VarpType *varp, int id, Packet *dat) {
    while (true) {
        int code = g1(dat);
        if (code == 0) {
            return;
        }

        if (code == 1) {
            varp->code1 = g1(dat);
        } else if (code == 2) {
            varp->code2 = g1(dat);
        } else if (code == 3) {
            varp->hasCode3 = true;
            _VarpType.code3[_VarpType.code3Count++] = id;
        } else if (code == 4) {
            varp->code4 = false;
        } else if (code == 5) {
            varp->clientcode = g2(dat);
        } else if (code == 6) {
            varp->code6 = true;
        } else if (code == 7) {
            varp->code7 = g4(dat);
        } else if (code == 8) {
            varp->code8 = true;
        } else if (code == 10) {
            varp->code10 = gjstr(dat);
        } else {
            rs2_error("Error unrecognised varp config code: %d\n", code);
        }
    }
}
