#include <stdlib.h>
#include <string.h>

#include "allocator.h"
#include "platform.h"

#ifdef __3DS__
#include <3ds.h>
#endif

typedef struct {
    int8_t *data;
    int capacity;
    int used;
} BumpAllocator;

static BumpAllocator alloc = {0};

static void *bump_alloc(int size);

int bump_allocator_used(void) {
    return alloc.used;
}

int bump_allocator_capacity(void) {
    return alloc.capacity;
}

bool bump_allocator_init(int capacity) {
#ifdef __3DS__
    // this large malloc fails on 3ds, so we use linearAlloc
    rs2_log("Free linear space: %d\n", linearSpaceFree());
    alloc.data = linearAlloc(capacity * sizeof(int8_t));
    rs2_log("Free linear space: %d\n", linearSpaceFree());
    if (alloc.data) {
        memset(alloc.data, 0, capacity);
    }
#else
    alloc.data = calloc(capacity, sizeof(int8_t));
#endif
    if (!alloc.data) {
        rs2_error("Failed to init allocator with size of: %d", capacity);
        return false;
    }
    alloc.capacity = capacity;
    alloc.used = 0;
    return true;
}

void bump_allocator_free(void) {
#ifdef __3DS__
    linearFree(alloc.data);
#else
    free(alloc.data);
#endif
}

void bump_allocator_reset(void) {
    // rs2_log("Allocator reset: clearing caches (size %d)", alloc.used);
    memset(alloc.data, 0, alloc.capacity);
    alloc.used = 0;
}

// returning malloc/calloc here effectively disables the allocator
void *rs2_malloc(bool use_allocator, int size) {
    // return malloc(size);
    return use_allocator ? bump_alloc(size) : malloc(size);
}

void *rs2_calloc(bool use_allocator, int count, int size) {
    // return calloc(count, size);
    // allocator_reset already memsets for us so we can just move ptr
    return use_allocator ? bump_alloc(count * size) : calloc(count, size);
}

static void *bump_alloc(int size) {
    // use 4 byte alignment to save couple dozen KBs memory, confirm it works everywhere
#if __SIZEOF_POINTER__ == 4
    int aligned_ptr = alloc.used + 3 & ~3;
#else
    int aligned_ptr = alloc.used + 7 & ~7;
#endif
    if (aligned_ptr + size > alloc.capacity) {
        rs2_error("Allocator full: this should never happen! attempted: %d, capacity: %d", alloc.used + size, alloc.capacity);
        exit(1);
    }

    void *next = alloc.data + aligned_ptr;
    alloc.used = aligned_ptr + size;
    return next;
}
