#pragma once

#include "linkable.h"

// name taken from jaclib
typedef struct LinkList {
    Linkable *sentinel;
    Linkable *cursor;
} LinkList;

LinkList *linklist_new(void);
void linklist_add_tail(LinkList *list, Linkable *node);
void linklist_add_head(LinkList *list, Linkable *node);
Linkable *linklist_remove_head(LinkList *list);
Linkable *linklist_head(LinkList *list);
Linkable *linklist_tail(LinkList *list);
Linkable *linklist_next(LinkList *list);
Linkable *linklist_prev(LinkList *list);
void linklist_clear(LinkList *list);
void linklist_free(LinkList *list);
