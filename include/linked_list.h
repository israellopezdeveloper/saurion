#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h> // for uint64_t

  struct Node;

  [[nodiscard]]
  int list_insert (struct Node **head, void *ptr, const uint64_t amount,
                   void *const *children);

  void list_delete_node (struct Node **head, const void *const ptr);

  void list_free (struct Node **head);

#ifdef __cplusplus
}
#endif

#endif // !LINKED_LIST_H
