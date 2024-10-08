#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

struct Node;

int list_insert(struct Node** head, void* ptr, size_t amount, void** children);

void list_delete_node(struct Node** head, void* ptr);

void list_free(struct Node** head);

#ifdef __cplusplus
}
#endif

#endif  // !LINKED_LIST_H
