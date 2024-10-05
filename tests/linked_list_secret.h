#ifndef LINKED_LIST_SECRET_H
#define LINKED_LIST_SECRET_H

#include <stdlib.h>

struct Node;

struct Node* create_node(void* ptr, size_t amount, void** children);

void free_node(struct Node* current);

#endif  // !LINKED_LIST_SECRET_H
