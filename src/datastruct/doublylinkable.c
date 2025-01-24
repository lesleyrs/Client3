#include <stddef.h>

#include "doublylinkable.h"

void doublylinkable_uncache(DoublyLinkable *link) {
    if (link->prev2) {
        link->prev2->next2 = link->next2;
        link->next2->prev2 = link->prev2;
        link->next2 = NULL;
        link->prev2 = NULL;
    }
}
