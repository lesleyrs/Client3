#pragma once

#include <stddef.h>

#include "model.h"
#include "npctype.h"
#include "pathingentity.h"

typedef struct {
    PathingEntity pathing_entity;
    NpcType *type;
} NpcEntity;

Model *npcentity_draw(NpcEntity *npc, int loopCycle);
Model *npcentity_get_sequencedmodel(NpcEntity *npc);
bool npcentity_is_visible(NpcEntity *npc);
NpcEntity *npcentity_new(void);
