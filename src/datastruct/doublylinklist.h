#pragma once

#include "doublylinkable.h"

typedef struct {
    DoublyLinkable *head;
} DoublyLinkList;

DoublyLinkList *doublylinklist_new(void);
void doublylinklist_push(DoublyLinkList *list, DoublyLinkable *node);
DoublyLinkable *doublylinklist_pop(DoublyLinkList *list);
void doublylinklist_free(DoublyLinkList *list);
