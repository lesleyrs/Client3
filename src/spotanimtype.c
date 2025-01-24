#include <stdlib.h>

#include "jagfile.h"
#include "packet.h"
#include "platform.h"
#include "seqtype.h"
#include "spotanimtype.h"

extern SeqTypeData _SeqType;

SpotAnimTypeData _SpotAnimType = {0};

static void spotanimtype_decode(SpotAnimType *spotanim, Packet *dat);

static SpotAnimType *spotanimtype_new(void) {
    SpotAnimType *spot = calloc(1, sizeof(SpotAnimType));
    spot->anim = -1;
    spot->resizeh = 128;
    spot->resizev = 128;
    return spot;
}

void spotanimtype_free_global(void) {
    lrucache_free(_SpotAnimType.modelCache);
    for (int i = 0; i < _SpotAnimType.count; i++) {
        free(_SpotAnimType.instances[i]);
    }
    free(_SpotAnimType.instances);
}

void spotanimtype_unpack(Jagfile *config) {
    Packet *dat = jagfile_to_packet(config, "spotanim.dat");
    _SpotAnimType.count = g2(dat);

    if (!_SpotAnimType.instances) {
        _SpotAnimType.instances = calloc(_SpotAnimType.count, sizeof(SpotAnimType *));
    }

    for (int id = 0; id < _SpotAnimType.count; id++) {
        if (!_SpotAnimType.instances[id]) {
            _SpotAnimType.instances[id] = spotanimtype_new();
        }

        _SpotAnimType.instances[id]->index = id;
        spotanimtype_decode(_SpotAnimType.instances[id], dat);
    }
    _SpotAnimType.modelCache = lrucache_new(30);

    packet_free(dat);
}

static void spotanimtype_decode(SpotAnimType *spotanim, Packet *dat) {
    while (true) {
        int code = g1(dat);
        if (code == 0) {
            break;
        }

        if (code == 1) {
            spotanim->model = g2(dat);
        } else if (code == 2) {
            spotanim->anim = g2(dat);

            if (_SeqType.instances) {
                spotanim->seq = _SeqType.instances[spotanim->anim];
            }
        } else if (code == 3) {
            spotanim->animHasAlpha = true;
        } else if (code == 4) {
            spotanim->resizeh = g2(dat);
        } else if (code == 5) {
            spotanim->resizev = g2(dat);
        } else if (code == 6) {
            spotanim->orientation = g2(dat);
        } else if (code == 7) {
            spotanim->ambient = g1(dat);
        } else if (code == 8) {
            spotanim->contrast = g1(dat);
        } else if (code >= 40 && code < 50) {
            spotanim->recol_s[code - 40] = g2(dat);
        } else if (code >= 50 && code < 60) {
            spotanim->recol_d[code - 50] = g2(dat);
        } else {
            rs2_error("Error unrecognised spotanim config code: %d\n", code);
        }
    }
}

Model *spotanimtype_get_model(SpotAnimType *spotanim) {
    Model *model = (Model *)lrucache_get(_SpotAnimType.modelCache, spotanim->index);
    if (model) {
        return model;
    }

    model = model_from_id(spotanim->model, true);
    for (int i = 0; i < 6; i++) {
        if (spotanim->recol_s[0] != 0) {
            model_recolor(model, spotanim->recol_s[i], spotanim->recol_d[i]);
        }
    }

    lrucache_put(_SpotAnimType.modelCache, spotanim->index, &model->link);
    return model;
}
