#include <stdlib.h>

#include "linkable.h"

void linkable_unlink(Linkable *link) {
    if (link->prev) {
        link->prev->next = link->next;
        link->next->prev = link->prev;
        link->next = NULL;
        link->prev = NULL;
    }
}
