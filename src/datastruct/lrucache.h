#pragma once

#include <stdint.h>

#include "doublylinklist.h"
#include "hashtable.h"

typedef struct {
    int capacity;
    int available;
    HashTable *hashtable;
    DoublyLinkList *history;
} LruCache;

LruCache *lrucache_new(int size);
DoublyLinkable *lrucache_get(LruCache *cache, int64_t key);
void lrucache_put(LruCache *cache, int64_t key, DoublyLinkable *value);
void lrucache_clear(LruCache *cache);
void lrucache_free(LruCache *cache);
