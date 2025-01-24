#pragma once

#include "datastruct/linkable.h"

typedef struct {
    Linkable link;
    int index;
    int count;
} ObjStackEntity;

ObjStackEntity *objstackentity_new(void);
