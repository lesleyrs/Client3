#include <stdlib.h>
#include <string.h>

#include "allocator.h"
#include "platform.h"

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

void bump_allocator_init(int capacity) {
    alloc.data = calloc(capacity, sizeof(int8_t));
    if (!alloc.data) {
        rs2_error("Failed to initialize allocater with size of: %d", capacity);
        exit(1);
    }
    alloc.capacity = capacity;
    alloc.used = 0;
}

void bump_allocator_free(void) {
    free(alloc.data);
}

void bump_allocator_reset(void) {
    // rs2_log("Allocator reset: clearing caches (size %d)", alloc.used);
    memset(alloc.data, 0, alloc.capacity);
    alloc.used = 0;
}

// NOTE: returning malloc/calloc here effectively disables the allocator
void *rs2_malloc(bool use_allocator, int size) {
    // return malloc(size);
    return use_allocator ? bump_alloc(size) : malloc(size);
}

void *rs2_calloc(bool use_allocator, int count, int size) {
    // return calloc(count, size);
    // NOTE: allocator_reset already memsets for us so we can just move ptr
    return use_allocator ? bump_alloc(count * size) : calloc(count, size);
}

static void *bump_alloc(int size) {
    // NOTE: why only sanitizers complain about alignment not valgrind?
    // int aligned_ptr = alloc.used + 3 & ~3; // NOTE 4 byte alignment may save some memory
    int aligned_ptr = alloc.used + 7 & ~7;
    if (aligned_ptr + size > alloc.capacity) {
        rs2_error("Allocator full: this should never happen! attempted: %d, capacity: %d", alloc.used + size, alloc.capacity);
        exit(1);
    }

    void *next = alloc.data + aligned_ptr;
    alloc.used = aligned_ptr + size;
    return next;
}
