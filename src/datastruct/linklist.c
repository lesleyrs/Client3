#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "linklist.h"

LinkList *linklist_new(void) {
    LinkList *list = calloc(1, sizeof(LinkList));
    list->sentinel = calloc(1, sizeof(Linkable));
    list->sentinel->next = list->sentinel;
    list->sentinel->prev = list->sentinel;
    return list;
}

void linklist_free(LinkList *list) {
    free(list->sentinel);
    free(list);
}

void linklist_add_tail(LinkList *list, Linkable *node) {
    if (node->prev) {
        linkable_unlink(node);
    }

    node->prev = list->sentinel->prev;
    node->next = list->sentinel;
    node->prev->next = node;
    node->next->prev = node;
}

void linklist_add_head(LinkList *list, Linkable *node) {
    if (node->prev) {
        linkable_unlink(node);
    }

    node->prev = list->sentinel;
    node->next = list->sentinel->next;
    node->prev->next = node;
    node->next->prev = node;
}

Linkable *linklist_remove_head(LinkList *list) {
    Linkable *node = list->sentinel->next;
    if (node == list->sentinel) {
        return NULL;
    }
    linkable_unlink(node);
    return node;
}

Linkable *linklist_head(LinkList *list) {
    Linkable *node = list->sentinel->next;
    if (node == list->sentinel) {
        list->cursor = NULL;
        return NULL;
    }
    list->cursor = node->next;
    return node;
}

Linkable *linklist_tail(LinkList *list) {
    Linkable *node = list->sentinel->prev;
    if (node == list->sentinel) {
        list->cursor = NULL;
        return NULL;
    }
    list->cursor = node->prev;
    return node;
}

Linkable *linklist_next(LinkList *list) {
    Linkable *node = list->cursor;
    if (node == list->sentinel) {
        list->cursor = NULL;
        return NULL;
    }
    list->cursor = node->next;
    return node;
}

Linkable *linklist_prev(LinkList *list) {
    Linkable *node = list->cursor;
    if (node == list->sentinel) {
        list->cursor = NULL;
        return NULL;
    }
    list->cursor = node->prev;
    return node;
}

void linklist_clear(LinkList *list) {
    while (true) {
        Linkable *node = list->sentinel->next;
        if (node == list->sentinel) {
            return;
        }
        linkable_unlink(node);
    }
}
