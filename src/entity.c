#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "datastruct/linkable.h"
#include "entity.h"
#include "npcentity.h"
#include "platform.h"
#include "playerentity.h"
#include "projectileentity.h"
#include "spotanimentity.h"

Entity *entity_new(const char *type) {
    Entity *entity = calloc(1, sizeof(Entity));
    entity->link = (Linkable){0};
    entity->type = type;
    return entity;
}

void entity_draw_free(Entity *entity, Model *m, int loopCycle) {
    if (strcmp(entity->type, "player") == 0) {
        PlayerEntity *player = ((PlayerEntity *)entity);
        if (!player->lowmem) {
            if ((player->pathing_entity.spotanimId != -1 && player->pathing_entity.spotanimFrame != -1) || (player->locModel && loopCycle >= player->locStartCycle && loopCycle < player->locStopCycle)) {
                model_free(m);
            } else {
                model_free_share_alpha(m, true);
            }
        }
    }
    if (strcmp(entity->type, "npc") == 0) {
        NpcEntity *npc = ((NpcEntity *)entity);
        if (npc->pathing_entity.spotanimId == -1 || npc->pathing_entity.spotanimFrame == -1) {
            model_free_share_alpha(m, !npc->type->animHasAlpha);
        } else {
            model_free(m);
        }
    }
    if (strcmp(entity->type, "spotanim") == 0) {
        SpotAnimEntity *spotanim = ((SpotAnimEntity *)entity);
        model_free_share_colored(m, true, !spotanim->type->animHasAlpha, false);
    }
    if (strcmp(entity->type, "projectile") == 0) {
        ProjectileEntity *projectile = ((ProjectileEntity *)entity);
        model_free_share_colored(m, true, !projectile->spotanim->animHasAlpha, false);
    }
}

Model *entity_draw(Entity *entity, int loopCycle) {
    Model *model = NULL;
    if (strcmp(entity->type, "player") == 0) {
        model = playerentity_draw((PlayerEntity *)entity, loopCycle);
    }
    if (strcmp(entity->type, "npc") == 0) {
        model = npcentity_draw((NpcEntity *)entity, loopCycle);
    }
    if (strcmp(entity->type, "spotanim") == 0) {
        model = spotanimentity_draw((SpotAnimEntity *)entity, loopCycle);
    }
    if (strcmp(entity->type, "projectile") == 0) {
        model = projectileentity_draw((ProjectileEntity *)entity, loopCycle);
    }
    return model;
}
