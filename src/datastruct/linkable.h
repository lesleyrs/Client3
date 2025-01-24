#pragma once

#include <stdint.h>

// name and packaging confirmed 100% in rs2/mapview applet strings
typedef struct Linkable Linkable;
struct Linkable {
    int64_t key;
    Linkable *next;
    Linkable *prev;
    int refcount;
};

void linkable_unlink(Linkable *link);
