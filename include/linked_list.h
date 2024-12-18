/*!
 * @defgroup LinkedList
 *
 * @brief A module for managing a thread-safe linked list.
 *
 * This module provides functions to create, insert, delete, and free nodes in
 * a linked list. It is thread-safe, using a mutex to ensure proper
 * synchronization.
 *
 * ### General Diagram of the Linked List:
 *
 * ```
 * Head -> [Node(ptr=A, size=2)] -> [Node(ptr=B, size=0)] -> NULL
 *         |                      |
 *         +-> children[0] -> [Child Node(ptr=C)] -> NULL
 *         +-> children[1] -> [Child Node(ptr=D)] -> NULL
 * ```
 *
 * ### Example Usage:
 *
 * ```c
 * #include "linked_list.h"
 * #include <stdio.h>
 *
 * int main() {
 *     struct Node *head = NULL;
 *     int data1 = 42, data2 = 24;
 *     void *children[] = {&data1, &data2};
 *
 *     if (list_insert(&head, (void *)&data1, 2, children) == SUCCESS_CODE) {
 *         printf("Node inserted successfully!\n");
 *     } else {
 *         printf("Error inserting node.\n");
 *     }
 *
 *     list_delete_node(&head, (void *)&data1);
 *
 *     list_free(&head);
 *     return 0;
 * }
 * ```
 *
 * @author Israel
 * @date 2024
 *
 * @{
 */
#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h> // for uint64_t

  /*!
   * @struct Node
   * @brief Represents a node in the linked list.
   *
   * Each node stores a pointer, a size, an array of child nodes,
   * and a pointer to the next node in the list.
   */
  struct Node;

  /*!
   * @brief Inserts a new node into the linked list.
   *
   * This function creates and inserts a new node with the given data into the
   * list.
   *
   * @param head Pointer to the head of the linked list.
   * @param ptr Pointer to the data to be stored in the new node.
   * @param amount Number of children nodes to allocate.
   * @param children Array of pointers to data for the children nodes.
   * @return `SUCCESS_CODE` on success, or `ERROR_CODE` on failure.
   *
   * ### Insert Operation Diagram:
   * ```
   * Initial state:
   * Head -> NULL
   *
   * After inserting a node:
   * Head -> [Node(ptr=A, size=2)] -> NULL
   *         |
   *         +-> children[0] -> [Child Node(ptr=C)] -> NULL
   *         +-> children[1] -> [Child Node(ptr=D)] -> NULL
   * ```
   */
  [[nodiscard]]
  int list_insert (struct Node **head, void *ptr, const uint64_t amount,
                   void **children);

  /*!
   * @brief Deletes a node from the linked list.
   *
   * This function removes the first node containing the specified data and
   * frees its memory.
   *
   * @param head Pointer to the head of the linked list.
   * @param ptr Pointer to the data to identify the node to delete.
   *
   * ### Delete Operation Diagram:
   * ```
   * Before deletion:
   * Head -> [Node(ptr=A, size=2)] -> [Node(ptr=B, size=0)] -> NULL
   *         |
   *         +-> children[0] -> [Child Node(ptr=C)] -> NULL
   *         +-> children[1] -> [Child Node(ptr=D)] -> NULL
   *
   * After deleting node with ptr=A:
   * Head -> [Node(ptr=B, size=0)] -> NULL
   * ```
   */
  void list_delete_node (struct Node **head, const void *const ptr);

  /*!
   * @brief Frees the entire linked list.
   *
   * This function traverses the linked list, freeing each node and its
   * associated resources.
   *
   * @param head Pointer to the head of the linked list.
   *
   * ### Free Operation Diagram:
   * ```
   * Before freeing:
   * Head -> [Node(ptr=A, size=2)] -> [Node(ptr=B, size=0)] -> NULL
   *         |
   *         +-> children[0] -> [Child Node(ptr=C)] -> NULL
   *         +-> children[1] -> [Child Node(ptr=D)] -> NULL
   *
   * After freeing:
   * Head -> NULL
   * ```
   */
  void list_free (struct Node **head);

#ifdef __cplusplus
}
#endif

#endif // !LINKED_LIST_H

/*!
 * @}
 */
