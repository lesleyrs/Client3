#include <stdbool.h>
#include <stdint.h>

int bump_allocator_used(void);
int bump_allocator_capacity(void);
void bump_allocator_init(int capacity);
void bump_allocator_free(void);
void bump_allocator_reset(void);
void *rs2_malloc(bool use_allocator, int size);
void *rs2_calloc(bool use_allocator, int count, int size);
