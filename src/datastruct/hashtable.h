#pragma once

#include "linkable.h"

// name taken from jaclib
typedef struct {
    int bucket_count;
    Linkable **buckets;
} HashTable;

HashTable *hashtable_new(int size);
Linkable *hashtable_get(HashTable *table, int64_t key);
void hashtable_put(HashTable *table, int64_t key, Linkable *value);
void hashtable_free(HashTable *table);
