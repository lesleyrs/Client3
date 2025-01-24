#include <stdlib.h>

#include "model.h"
#include "spotanimentity.h"
#include "spotanimtype.h"

extern SpotAnimTypeData _SpotAnimType;

SpotAnimEntity *spotanimentity_new(int id, int level, int x, int z, int y, int cycle, int delay) {
    SpotAnimEntity *entity = calloc(1, sizeof(SpotAnimEntity));
    entity->entity = (Entity){(Linkable){0}, "spotanim"},
    entity->type = _SpotAnimType.instances[id];
    entity->level = level;
    entity->x = x;
    entity->z = z;
    entity->y = y;
    entity->startCycle = cycle + delay;
    entity->seqComplete = false;
    return entity;
}

void spotanimentity_update(SpotAnimEntity *entity, int delta) {
    for (entity->seqCycle += delta; entity->seqCycle > entity->type->seq->delay[entity->seqFrame];) {
        entity->seqCycle -= entity->type->seq->delay[entity->seqFrame] + 1;
        entity->seqFrame++;

        if (entity->seqFrame >= entity->type->seq->frameCount) {
            entity->seqFrame = 0;
            entity->seqComplete = true;
        }
    }
}

Model *spotanimentity_draw(SpotAnimEntity *entity, int loopCycle) {
    (void)loopCycle;
    Model *model = model_share_colored(spotanimtype_get_model(entity->type), true, !entity->type->animHasAlpha, false, false);

    if (!entity->seqComplete) {
        model_create_label_references(model, false);
        model_apply_transform(model, entity->type->seq->frames[entity->seqFrame]);
        model_free_label_references(model);
        model->label_faces = NULL;
        model->label_vertices = NULL;
    }

    if (entity->type->resizeh != 128 || entity->type->resizev != 128) {
        model_scale(model, entity->type->resizeh, entity->type->resizev, entity->type->resizeh);
    }

    if (entity->type->orientation != 0) {
        if (entity->type->orientation == 90) {
            model_rotate_y90(model);
        } else if (entity->type->orientation == 180) {
            model_rotate_y90(model);
            model_rotate_y90(model);
        } else if (entity->type->orientation == 270) {
            model_rotate_y90(model);
            model_rotate_y90(model);
            model_rotate_y90(model);
        }
    }

    model_calculate_normals(model, 64 + entity->type->ambient, 850 + entity->type->contrast, -30, -50, -30, true, false);
    return model;
}
