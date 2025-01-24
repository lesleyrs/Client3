#include <stddef.h>
#include <stdlib.h>

#include "model.h"
#include "npcentity.h"
#include "npctype.h"
#include "spotanimtype.h"

extern SeqTypeData _SeqType;
extern SpotAnimTypeData _SpotAnimType;

NpcEntity *npcentity_new(void) {
    NpcEntity *npc = calloc(1, sizeof(NpcEntity));
    npc->pathing_entity = pathingentity_new("npc");
    return npc;
}

Model *npcentity_draw(NpcEntity *npc, int loopCycle) {
    (void)loopCycle;
    if (!npc->type) {
        return NULL;
    }

    if (npc->pathing_entity.spotanimId == -1 || npc->pathing_entity.spotanimFrame == -1) {
        return npcentity_get_sequencedmodel(npc);
    }

    Model *model = npcentity_get_sequencedmodel(npc);
    SpotAnimType *spotanim = _SpotAnimType.instances[npc->pathing_entity.spotanimId];

    Model *model1 = model_share_colored(spotanimtype_get_model(spotanim), true, !spotanim->animHasAlpha, false, false);
    model_translate(model1, -npc->pathing_entity.spotanimOffset, 0, 0);
    model_create_label_references(model1, false);
    model_apply_transform(model1, spotanim->seq->frames[npc->pathing_entity.spotanimFrame]);
    model_free_label_references(model1);
    model1->label_faces = NULL;
    model1->label_vertices = NULL;

    if (spotanim->resizeh != 128 || spotanim->resizev != 128) {
        model_scale(model1, spotanim->resizeh, spotanim->resizev, spotanim->resizeh);
    }

    model_calculate_normals(model1, 64 + spotanim->ambient, 850 + spotanim->contrast, -30, -50, -30, true, false);

    Model *models[] = {model, model1};
    Model *tmp = model_from_models_bounds(models, 2);
    model_free_calculate_normals(model1);
    model_free_share_alpha(model, !npc->type->animHasAlpha);
    model_free_share_colored(model1, true, !spotanim->animHasAlpha, false);

    if (npc->type->size == 1) {
        tmp->pickable = true;
    }

    return tmp;
}

Model *npcentity_get_sequencedmodel(NpcEntity *npc) {
    if (npc->pathing_entity.primarySeqId >= 0 && npc->pathing_entity.primarySeqDelay == 0) {
        int primaryTransformId = _SeqType.instances[npc->pathing_entity.primarySeqId]->frames[npc->pathing_entity.primarySeqFrame];
        int secondaryTransformId = -1;
        if (npc->pathing_entity.secondarySeqId >= 0 && npc->pathing_entity.secondarySeqId != npc->pathing_entity.seqStandId) {
            secondaryTransformId = _SeqType.instances[npc->pathing_entity.secondarySeqId]->frames[npc->pathing_entity.secondarySeqFrame];
        }
        return npctype_get_sequencedmodel(npc->type, primaryTransformId, secondaryTransformId, _SeqType.instances[npc->pathing_entity.primarySeqId]->walkmerge);
    }

    int transformId = -1;
    if (npc->pathing_entity.secondarySeqId >= 0) {
        transformId = _SeqType.instances[npc->pathing_entity.secondarySeqId]->frames[npc->pathing_entity.secondarySeqFrame];
    }

    Model *model = npctype_get_sequencedmodel(npc->type, transformId, -1, NULL);
    npc->pathing_entity.height = model->max_y;
    return model;
}

bool npcentity_is_visible(NpcEntity *npc) {
    return npc->type != NULL;
}
