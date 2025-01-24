#include <stdio.h>
#include <stdlib.h>

#include "allocator.h"
#include "idktype.h"
#include "jagfile.h"
#include "model.h"
#include "packet.h"
#include "platform.h"

IdkTypeData _IdkType = {0};

static void idktype_decode(IdkType *idk, Packet *dat);

static IdkType *idktype_new(void) {
    IdkType *idk = calloc(1, sizeof(IdkType));
    idk->type = -1;
    for (int i = 0; i < 5; i++) {
        idk->heads[i] = -1;
    }
    return idk;
}

void idktype_free_global(void) {
    for (int i = 0; i < _IdkType.count; i++) {
        free(_IdkType.instances[i]->models);
        free(_IdkType.instances[i]);
    }
    free(_IdkType.instances);
}

void idktype_unpack(Jagfile *config) {
    Packet *dat = jagfile_to_packet(config, "idk.dat");
    _IdkType.count = g2(dat);

    if (!_IdkType.instances) {
        _IdkType.instances = calloc(_IdkType.count, sizeof(IdkType *));
    }

    for (int id = 0; id < _IdkType.count; id++) {
        if (!_IdkType.instances[id]) {
            _IdkType.instances[id] = idktype_new();
        }

        idktype_decode(_IdkType.instances[id], dat);
    }

    packet_free(dat);
}

static void idktype_decode(IdkType *idk, Packet *dat) {
    while (true) {
        int code = g1(dat);
        if (code == 0) {
            break;
        }

        if (code == 1) {
            idk->type = g1(dat);
        } else if (code == 2) {
            int count = g1(dat);
            idk->models_count = count;
            idk->models = calloc(count, sizeof(int));

            for (int i = 0; i < count; i++) {
                idk->models[i] = g2(dat);
            }
        } else if (code == 3) {
            idk->disable = true;
        } else if (code >= 40 && code < 50) {
            idk->recol_s[code - 40] = g2(dat);
        } else if (code >= 50 && code < 60) {
            idk->recol_d[code - 50] = g2(dat);
        } else if (code >= 60 && code < 70) {
            idk->heads[code - 60] = g2(dat);
        } else {
            rs2_error("Error unrecognised idk config code: %d\n", code);
        }
    }
}

Model *idktype_get_model(IdkType *idk) {
    if (!idk->models) {
        return NULL;
    }

    Model **models = calloc(idk->models_count, sizeof(Model *));
    for (int i = 0; i < idk->models_count; i++) {
        models[i] = model_from_id(idk->models[i], false);
    }

    Model *model;
    if (idk->models_count == 1) {
        model = models[0];
    } else {
        model = model_from_models(models, idk->models_count, false);
        for (int i = 0; i < idk->models_count; i++) {
            model_free(models[i]);
        }
    }
    free(models);

    for (int i = 0; i < 6 && idk->recol_s[i] != 0; i++) {
        model_recolor(model, idk->recol_s[i], idk->recol_d[i]);
    }

    return model;
}

Model *idktype_get_headmodel(IdkType *idk) {
    Model *models[5] = {0};

    int count = 0;
    for (int i = 0; i < 5; i++) {
        if (idk->heads[i] != -1) {
            models[count++] = model_from_id(idk->heads[i], false);
        }
    }

    Model *model = model_from_models(models, count, false);
    for (int i = 0; i < 6 && idk->recol_s[i] != 0; i++) {
        model_recolor(model, idk->recol_s[i], idk->recol_d[i]);
    }

    return model;
}
