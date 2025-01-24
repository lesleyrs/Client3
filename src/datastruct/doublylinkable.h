#pragma once

#include "linkable.h"

typedef struct DoublyLinkable DoublyLinkable;
struct DoublyLinkable {
    Linkable link;
    DoublyLinkable *next2;
    DoublyLinkable *prev2;
};

void doublylinkable_uncache(DoublyLinkable *link);
