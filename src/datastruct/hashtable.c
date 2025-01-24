#include <stdlib.h>

#include "hashtable.h"
#include "linkable.h"

HashTable *hashtable_new(int size) {
    HashTable *table = calloc(1, sizeof(HashTable));
    table->buckets = calloc(size, sizeof(Linkable *));
    table->bucket_count = size;

    for (int i = 0; i < size; i++) {
        Linkable *sentinel = table->buckets[i] = calloc(1, sizeof(Linkable));
        sentinel->next = sentinel;
        sentinel->prev = sentinel;
    }

    return table;
}

void hashtable_free(HashTable *table) {
    for (int i = 0; i < table->bucket_count; i++) {
        free(table->buckets[i]);
    }
    free(table->buckets);
    free(table);
}

Linkable *hashtable_get(HashTable *table, int64_t key) {
    Linkable *sentinel = table->buckets[(int)(key & (int64_t)(table->bucket_count - 1))];

    for (Linkable *node = sentinel->next; node != sentinel; node = node->next) {
        if (node->key == key) {
            return node;
        }
    }

    return NULL;
}

void hashtable_put(HashTable *table, int64_t key, Linkable *value) {
    if (value->prev) {
        linkable_unlink(value);
    }

    Linkable *sentinel = table->buckets[(int)(key & (int64_t)(table->bucket_count - 1))];
    value->prev = sentinel->prev;
    value->next = sentinel;
    value->prev->next = value;
    value->next->prev = value;
    value->key = key;
}
