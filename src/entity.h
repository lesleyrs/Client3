#pragma once

#include <stddef.h>

#include "datastruct/linkable.h"
#include "model.h"

typedef struct {
    Linkable link;
    const char *type;
} Entity;

Entity *entity_new(const char *type);
Model *entity_draw(Entity *entity, int loopCycle);
void entity_draw_free(Entity *entity, Model *m, int loopCycle);
