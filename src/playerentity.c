#include "playerentity.h"
#include "datastruct/jstring.h"
#include "datastruct/lrucache.h"
#include "defines.h"
#include "idktype.h"
#include "model.h"
#include "objtype.h"
#include "packet.h"
#include "pathingentity.h"
#include "seqtype.h"
#include "spotanimtype.h"
#include <stdlib.h>
#include <string.h>

extern IdkTypeData _IdkType;
extern SeqTypeData _SeqType;
extern SpotAnimTypeData _SpotAnimType;

int DESIGN_HAIR_COLOR[] = {
    9104, 10275, 7595, 3610, 7975, 8526, 918, 38802, 24466, 10145, 58654, 5027, 1457, 16565, 34991, 25486};

int *DESIGN_BODY_COLOR[] = {
    // hair
    (int[]){BODY_RECOLOR_KHAKI, BODY_RECOLOR_CHARCOAL, BODY_RECOLOR_CRIMSON, BODY_RECOLOR_NAVY, BODY_RECOLOR_STRAW, BODY_RECOLOR_WHITE, BODY_RECOLOR_RED, BODY_RECOLOR_BLUE, BODY_RECOLOR_GREEN, BODY_RECOLOR_YELLOW, BODY_RECOLOR_PURPLE, BODY_RECOLOR_ORANGE, BODY_RECOLOR_ROSE, BODY_RECOLOR_LIME, BODY_RECOLOR_CYAN, BODY_RECOLOR_EMERALD},
    // torso
    (int[]){BODY_KHAKI, BODY_CHARCOAL, BODY_CRIMSON, BODY_NAVY, BODY_STRAW, BODY_WHITE, BODY_RED, BODY_BLUE, BODY_GREEN, BODY_YELLOW, BODY_PURPLE, BODY_ORANGE, BODY_ROSE, BODY_LIME, BODY_CYAN, BODY_EMERALD},
    // legs
    (int[]){BODY_EMERALD - 1, BODY_KHAKI + 1, BODY_CHARCOAL, BODY_CRIMSON, BODY_NAVY, BODY_STRAW, BODY_WHITE, BODY_RED, BODY_BLUE, BODY_GREEN, BODY_YELLOW, BODY_PURPLE, BODY_ORANGE, BODY_ROSE, BODY_LIME, BODY_CYAN},
    // feet
    (int[]){FEET_BROWN, FEET_KHAKI, FEET_ASHEN, FEET_DARK, FEET_TERRACOTTA, FEET_GREY},
    // skin
    (int[]){SKIN_DARKER, SKIN_DARKER_DARKER, SKIN_DARKER_DARKER_DARKER, SKIN_DARKER_DARKER_DARKER_DARKER, SKIN_DARKER_DARKER_DARKER_DARKER_DARKER, SKIN_DARKER_DARKER_DARKER_DARKER_DARKER_DARKER, SKIN_DARKER_DARKER_DARKER_DARKER_DARKER_DARKER_DARKER, SKIN}};

int DESIGN_BODY_COLOR_LENGTH[] = {16, 16, 16, 6, 8};

PlayerEntityData _PlayerEntity = {0};

PlayerEntity *playerentity_new(void) {
    PlayerEntity *player = calloc(1, sizeof(PlayerEntity));
    player->pathing_entity = pathingentity_new("player");
    return player;
}

void playerentity_init_global(void) {
    _PlayerEntity.modelCache = lrucache_new(200);
}

void playerentity_free_global(void) {
    lrucache_free(_PlayerEntity.modelCache);
}

void playerentity_read(PlayerEntity *entity, Packet *buf) {
    buf->pos = 0;

    entity->gender = g1(buf);
    entity->headicons = g1(buf);

    for (int part = 0; part < 12; part++) {
        int msb = g1(buf);
        if (msb == 0) {
            entity->appearances[part] = 0;
        } else {
            int lsb = g1(buf);
            entity->appearances[part] = (msb << 8) + lsb;
        }
    }

    for (int part = 0; part < 5; part++) {
        int color = g1(buf);
        if (color < 0 || color >= DESIGN_BODY_COLOR_LENGTH[part]) {
            color = 0;
        }

        entity->colors[part] = color;
    }

    entity->pathing_entity.seqStandId = g2(buf);
    if (entity->pathing_entity.seqStandId == 65535) {
        entity->pathing_entity.seqStandId = -1;
    }

    entity->pathing_entity.seqTurnId = g2(buf);
    if (entity->pathing_entity.seqTurnId == 65535) {
        entity->pathing_entity.seqTurnId = -1;
    }

    entity->pathing_entity.seqWalkId = g2(buf);
    if (entity->pathing_entity.seqWalkId == 65535) {
        entity->pathing_entity.seqWalkId = -1;
    }

    entity->pathing_entity.seqTurnAroundId = g2(buf);
    if (entity->pathing_entity.seqTurnAroundId == 65535) {
        entity->pathing_entity.seqTurnAroundId = -1;
    }

    entity->pathing_entity.seqTurnLeftId = g2(buf);
    if (entity->pathing_entity.seqTurnLeftId == 65535) {
        entity->pathing_entity.seqTurnLeftId = -1;
    }

    entity->pathing_entity.seqTurnRightId = g2(buf);
    if (entity->pathing_entity.seqTurnRightId == 65535) {
        entity->pathing_entity.seqTurnRightId = -1;
    }

    entity->pathing_entity.seqRunId = g2(buf);
    if (entity->pathing_entity.seqRunId == 65535) {
        entity->pathing_entity.seqRunId = -1;
    }

    char* name = jstring_format_name(jstring_from_base37(g8(buf)));
    strcpy(entity->name, name);
    free(name);
    entity->combatLevel = g1(buf);

    entity->visible = true;
    entity->appearanceHashcode = 0L;
    for (int part = 0; part < 12; part++) {
        entity->appearanceHashcode <<= 0x4;

        if (entity->appearances[part] >= 256) {
            entity->appearanceHashcode += entity->appearances[part] - 256;
        }
    }

    if (entity->appearances[0] >= 256) {
        entity->appearanceHashcode += (entity->appearances[0] - 256) >> 4;
    }

    if (entity->appearances[1] >= 256) {
        entity->appearanceHashcode += (entity->appearances[1] - 256) >> 8;
    }

    for (int part = 0; part < 5; part++) {
        entity->appearanceHashcode <<= 0x3;
        entity->appearanceHashcode += entity->colors[part];
    }

    entity->appearanceHashcode <<= 0x1;
    entity->appearanceHashcode += entity->gender;
}

Model *playerentity_draw(PlayerEntity *entity, int loopCycle) {
    if (!entity->visible) {
        return NULL;
    }

    Model *model = playerentity_get_sequencedmodel(entity);
    entity->pathing_entity.height = model->max_y;
    model->pickable = true;

    if (entity->lowmem) {
        return model;
    }

    if (entity->pathing_entity.spotanimId != -1 && entity->pathing_entity.spotanimFrame != -1) {
        SpotAnimType *spotanim = _SpotAnimType.instances[entity->pathing_entity.spotanimId];
        Model *model2 = model_share_colored(spotanimtype_get_model(spotanim), true, !spotanim->animHasAlpha, false, false);

        model_translate(model2, -entity->pathing_entity.spotanimOffset, 0, 0);
        model_create_label_references(model2, false);
        model_apply_transform(model2, spotanim->seq->frames[entity->pathing_entity.spotanimFrame]);
        model_free_label_references(model2);
        model2->label_faces = NULL;
        model2->label_vertices = NULL;
        if (spotanim->resizeh != 128 || spotanim->resizev != 128) {
            model_scale(model2, spotanim->resizeh, spotanim->resizev, spotanim->resizeh);
        }
        model_calculate_normals(model2, spotanim->ambient + 64, spotanim->contrast + 850, -30, -50, -30, true, false);

        Model *models[] = {model, model2};
        model = model_from_models_bounds(models, 2);
        model_free_calculate_normals(model2);
        model_free_share_alpha(models[0], true); // to get the original model before it gets re-assigned
        model_free_share_colored(model2, true, !spotanim->animHasAlpha, false);
    }

    if (entity->locModel) {
        if (loopCycle >= entity->locStopCycle) {
            entity->locModel = NULL;
        }

        if (loopCycle >= entity->locStartCycle && loopCycle < entity->locStopCycle) {
            Model *loc = entity->locModel;
            model_translate(loc, entity->locOffsetY - entity->y, entity->locOffsetX - entity->pathing_entity.x, entity->locOffsetZ - entity->pathing_entity.z);
            if (entity->pathing_entity.dstYaw == 512) {
                model_rotate_y90(loc);
                model_rotate_y90(loc);
                model_rotate_y90(loc);
            } else if (entity->pathing_entity.dstYaw == 1024) {
                model_rotate_y90(loc);
                model_rotate_y90(loc);
            } else if (entity->pathing_entity.dstYaw == 1536) {
                model_rotate_y90(loc);
            }

            Model *models[] = {model, loc};
            model = model_from_models_bounds(models, 2);
            if (entity->pathing_entity.spotanimId != -1 && entity->pathing_entity.spotanimFrame != -1) {
                model_free(models[0]);
            } else {
                model_free_share_alpha(models[0], true);
            }

            if (entity->pathing_entity.dstYaw == 512) {
                model_rotate_y90(loc);
            } else if (entity->pathing_entity.dstYaw == 1024) {
                model_rotate_y90(loc);
                model_rotate_y90(loc);
            } else if (entity->pathing_entity.dstYaw == 1536) {
                model_rotate_y90(loc);
                model_rotate_y90(loc);
                model_rotate_y90(loc);
            }

            model_translate(loc, entity->y - entity->locOffsetY, entity->pathing_entity.x - entity->locOffsetX, entity->pathing_entity.z - entity->locOffsetZ);
        }
    }

    model->pickable = true;
    return model;
}

Model *playerentity_get_sequencedmodel(PlayerEntity *entity) {
    int64_t hashCode = entity->appearanceHashcode;
    int primaryTransformId = -1;
    int secondaryTransformId = -1;
    int rightHandValue = -1;
    int leftHandValue = -1;

    if (entity->pathing_entity.primarySeqId >= 0 && entity->pathing_entity.primarySeqDelay == 0) {
        SeqType *seq = _SeqType.instances[entity->pathing_entity.primarySeqId];

        primaryTransformId = seq->frames[entity->pathing_entity.primarySeqFrame];
        if (entity->pathing_entity.secondarySeqId >= 0 && entity->pathing_entity.secondarySeqId != entity->pathing_entity.seqStandId) {
            secondaryTransformId = _SeqType.instances[entity->pathing_entity.secondarySeqId]->frames[entity->pathing_entity.secondarySeqFrame];
        }

        if (seq->righthand >= 0) {
            rightHandValue = seq->righthand;
            hashCode += ((int64_t)rightHandValue - entity->appearances[5]) << 8;
        }

        if (seq->lefthand >= 0) {
            leftHandValue = seq->lefthand;
            hashCode += ((int64_t)leftHandValue - entity->appearances[3]) << 16;
        }
    } else if (entity->pathing_entity.secondarySeqId >= 0) {
        primaryTransformId = _SeqType.instances[entity->pathing_entity.secondarySeqId]->frames[entity->pathing_entity.secondarySeqFrame];
    }

    Model *model = (Model *)lrucache_get(_PlayerEntity.modelCache, hashCode);
    if (!model) {
        Model *models[12] = {0};
        int modelCount = 0;

        for (int part = 0; part < 12; part++) {
            int value = entity->appearances[part];

            if (leftHandValue >= 0 && part == 3) {
                value = leftHandValue;
            }

            if (rightHandValue >= 0 && part == 5) {
                value = rightHandValue;
            }

            if (value >= 256 && value < 512) {
                models[modelCount++] = idktype_get_model(_IdkType.instances[value - 256]);
            }

            if (value >= 512) {
                ObjType *obj = objtype_get(value - 512);
                Model *wornModel = objtype_get_wornmodel(obj, entity->gender);

                if (wornModel) {
                    models[modelCount++] = wornModel;
                }
            }
        }

        model = model_from_models(models, modelCount, true);
        for (int part = 0; part < 12; part++) {
            if (models[part]) {
                model_free(models[part]);
            }
        }

        for (int part = 0; part < 5; part++) {
            if (entity->colors[part] != 0) {
                model_recolor(model, DESIGN_BODY_COLOR[part][0], DESIGN_BODY_COLOR[part][entity->colors[part]]);

                if (part == 1) {
                    model_recolor(model, DESIGN_HAIR_COLOR[0], DESIGN_HAIR_COLOR[entity->colors[part]]);
                }
            }
        }

        model_create_label_references(model, true);
        model_calculate_normals(model, 64, 850, -30, -50, -30, true, true);
        lrucache_put(_PlayerEntity.modelCache, hashCode, &model->link);
    }

    if (entity->lowmem) {
        return model;
    }

    Model *tmp = model_share_alpha(model, true);
    if (primaryTransformId != -1 && secondaryTransformId != -1) {
        model_apply_transforms(tmp, primaryTransformId, secondaryTransformId, _SeqType.instances[entity->pathing_entity.primarySeqId]->walkmerge);
    } else if (primaryTransformId != -1) {
        model_apply_transform(tmp, primaryTransformId);
    }

    model_calculate_bounds_cylinder(tmp);
    tmp->label_faces = NULL;
    tmp->label_vertices = NULL;
    return tmp;
}

Model *playerentity_get_headmodel(PlayerEntity *entity) {
    if (!entity->visible) {
        return NULL;
    }

    Model *models[12] = {0};
    int modelCount = 0;
    for (int part = 0; part < 12; part++) {
        int value = entity->appearances[part];

        if (value >= 256 && value < 512) {
            models[modelCount++] = idktype_get_headmodel(_IdkType.instances[value - 256]);
        }

        if (value >= 512) {
            Model *headModel = objtype_get_headmodel(objtype_get(value - 512), entity->gender);

            if (headModel) {
                models[modelCount++] = headModel;
            }
        }
    }

    Model *tmp = model_from_models(models, modelCount, false);
    for (int part = 0; part < 5; part++) {
        if (entity->colors[part] != 0) {
            model_recolor(tmp, DESIGN_BODY_COLOR[part][0], DESIGN_BODY_COLOR[part][entity->colors[part]]);

            if (part == 1) {
                model_recolor(tmp, DESIGN_HAIR_COLOR[0], DESIGN_HAIR_COLOR[entity->colors[part]]);
            }
        }
    }

    return tmp;
}

bool playerentity_is_visible(PlayerEntity *entity) {
    return entity->visible;
}
