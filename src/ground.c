#include <stdlib.h>

#include "ground.h"
#include "tileoverlay.h"

Ground *ground_new(int level, int x, int z) {
    Ground *ground = calloc(1, sizeof(Ground));
    ground->link = (Linkable){0};
    ground->occludeLevel = ground->level = level;
    ground->x = x;
    ground->z = z;
    return ground;
}

void ground_free(Ground *ground) {
    if (ground->link.next) {
        free(ground->link.next);
    }
    if (ground->link.prev) {
        free(ground->link.prev);
    }
    free(ground->underlay);
    if (ground->overlay) {
        tileoverlay_free(ground->overlay);
    }
    free(ground->wall);
    free(ground->decor);
    free(ground->groundDecor);
    free(ground->groundObj);
    for (int loc = 0; loc < 5; loc++) {
        ground->locs[loc] = NULL;
    }

    if (ground && ground->bridge) {
        ground_free(ground->bridge);
    }
    free(ground);
}
