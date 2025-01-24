#include <stdlib.h>

#include "doublylinklist.h"

DoublyLinkList *doublylinklist_new(void) {
    DoublyLinkList *list = calloc(1, sizeof(DoublyLinkList));
    list->head = calloc(1, sizeof(DoublyLinkable));
    list->head->next2 = list->head;
    list->head->prev2 = list->head;
    return list;
}

void doublylinklist_free(DoublyLinkList *list) {
    free(list->head);
    free(list);
}

void doublylinklist_push(DoublyLinkList *list, DoublyLinkable *node) {
    if (node->prev2) {
        doublylinkable_uncache(node);
    }

    node->prev2 = list->head->prev2;
    node->next2 = list->head;
    node->prev2->next2 = node;
    node->next2->prev2 = node;
}

DoublyLinkable *doublylinklist_pop(DoublyLinkList *list) {
    DoublyLinkable *node = list->head->next2;
    if (node == list->head) {
        return NULL;
    } else {
        doublylinkable_uncache(node);
        return node;
    }
}
