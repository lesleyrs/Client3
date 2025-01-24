#include <stdlib.h>

#include "objstackentity.h"

ObjStackEntity *objstackentity_new(void) {
    ObjStackEntity *entity = calloc(1, sizeof(ObjStackEntity));
    entity->link = (Linkable){0};
    return entity;
}
