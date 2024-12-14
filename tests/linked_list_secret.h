#ifndef LINKED_LIST_SECRET_H
#define LINKED_LIST_SECRET_H

#include <cstdint> // for uint64_t

struct Node;

struct Node *create_node (void *ptr, uint64_t amount, void **children);

void free_node (struct Node *current);

#endif // !LINKED_LIST_SECRET_H
