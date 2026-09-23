#include <math.h>
#include <stdlib.h>

#include "projectileentity.h"
#include "defines.h"
#include "spotanimtype.h"

extern SpotAnimTypeData _SpotAnimType;

ProjectileEntity *projectileentity_new(int spotanim, int level, int srcX, int srcY, int srcZ, int startCycle, int lastCycle, int peakPitch, int arc, int target, int offsetY) {
    ProjectileEntity *entity = calloc(1, sizeof(ProjectileEntity));
    entity->entity = (Entity){(Linkable){0}, "projectile"},
    entity->spotanim = _SpotAnimType.instances[spotanim];
    entity->level = level;
    entity->srcX = srcX;
    entity->srcZ = srcZ;
    entity->srcY = srcY;
    entity->startCycle = startCycle;
    entity->lastCycle = lastCycle;
    entity->peakPitch = peakPitch;
    entity->arc = arc;
    entity->target = target;
    entity->offsetY = offsetY;
    entity->mobile = false;
    return entity;
}

void projectileentity_update_velocity(ProjectileEntity *entity, int dstX, int dstY, int dstZ, int cycle) {
    if (!entity->mobile) {
        DOUBLE dx = dstX - entity->srcX;
        DOUBLE dz = dstZ - entity->srcZ;
        DOUBLE d = SQRT(dx * dx + dz * dz);

        entity->x = (DOUBLE)entity->srcX + dx * (DOUBLE)entity->arc / d;
        entity->z = (DOUBLE)entity->srcZ + dz * (DOUBLE)entity->arc / d;
        entity->y = entity->srcY;
    }

    DOUBLE dt = entity->lastCycle + 1 - cycle;
    entity->velocityX = ((DOUBLE)dstX - entity->x) / dt;
    entity->velocityZ = ((DOUBLE)dstZ - entity->z) / dt;
    entity->velocity = SQRT(entity->velocityX * entity->velocityX + entity->velocityZ * entity->velocityZ);

    if (!entity->mobile) {
        entity->velocityY = -entity->velocity * TAN((DOUBLE)entity->peakPitch * 0.02454369);
    }

    entity->accelerationY = ((DOUBLE)dstY - entity->y - entity->velocityY * dt) * 2.0 / (dt * dt);
}

void projectileentity_update(ProjectileEntity *entity, int delta) {
    entity->mobile = true;
    entity->x += entity->velocityX * (DOUBLE)delta;
    entity->z += entity->velocityZ * (DOUBLE)delta;
    entity->y += entity->velocityY * (DOUBLE)delta + entity->accelerationY * 0.5 * (DOUBLE)delta * (DOUBLE)delta;
    entity->velocityY += entity->accelerationY * (DOUBLE)delta;
    entity->yaw = (int)(ATAN2(entity->velocityX, entity->velocityZ) * RADIANS_TO_RS) + 1024 & 0x7ff;
    entity->pitch = (int)(ATAN2(entity->velocityY, entity->velocity) * RADIANS_TO_RS) & 0x7ff;

    if (entity->spotanim->seq) {
        entity->seqCycle += delta;

        while (entity->seqCycle > entity->spotanim->seq->delay[entity->seqFrame]) {
            entity->seqCycle -= entity->spotanim->seq->delay[entity->seqFrame] + 1;
            entity->seqFrame++;
            if (entity->seqFrame >= entity->spotanim->seq->frameCount) {
                entity->seqFrame = 0;
            }
        }
    }
}

Model *projectileentity_draw(ProjectileEntity *entity, int loopCycle) {
    (void)loopCycle;
    Model *model = model_share_colored(spotanimtype_get_model(entity->spotanim), true, !entity->spotanim->animHasAlpha, false, false);

    if (entity->spotanim->seq) {
        model_create_label_references(model, false);
        model_apply_transform(model, entity->spotanim->seq->frames[entity->seqFrame]);
        model_free_label_references(model);
        model->label_faces = NULL;
        model->label_vertices = NULL;
    }

    if (entity->spotanim->resizeh != 128 || entity->spotanim->resizev != 128) {
        model_scale(model, entity->spotanim->resizeh, entity->spotanim->resizev, entity->spotanim->resizeh);
    }

    model_rotate_x(model, entity->pitch);
    model_calculate_normals(model, 64 + entity->spotanim->ambient, 850 + entity->spotanim->contrast, -30, -50, -30, true, false);
    return model;
}
