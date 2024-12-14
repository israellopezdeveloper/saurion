#include "linked_list.h"
#include "config.h" // for ERROR_CODE, SUCCESS_CODE

#include <pthread.h> // for pthread_mutex_lock, pthread_mutex_unlock, PTHREAD_MUTEX_INITIALIZER
#include <stdlib.h> // for malloc, free

struct Node
{
  void *ptr;
  uint64_t size;
  struct Node **children;
  struct Node *next;
};

pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

// create_node
[[nodiscard]]
struct Node *
create_node (void *ptr, const uint64_t amount, void *const *children)
{
  struct Node *new_node = (struct Node *)malloc (sizeof (struct Node));
  if (!new_node)
    {
      return NULL;
    }
  new_node->ptr = ptr;
  new_node->size = amount;
  new_node->children = NULL;
  if (amount <= 0)
    {
      new_node->next = NULL;
      return new_node;
    }
  new_node->children
      = (struct Node **)malloc (sizeof (struct Node *) * amount);
  if (!new_node->children)
    {
      free (new_node);
      return NULL;
    }
  for (uint64_t i = 0; i < amount; ++i)
    {
      new_node->children[i] = (struct Node *)malloc (sizeof (struct Node));

      if (!new_node->children[i])
        {
          for (uint64_t j = 0; j < i; ++j)
            {
              free (new_node->children[j]);
            }
          free (new_node);
          return NULL;
        }
    }
  for (uint64_t i = 0; i < amount; ++i)
    {
      new_node->children[i]->size = 0;
      new_node->children[i]->next = NULL;
      new_node->children[i]->ptr = children[i];
      new_node->children[i]->children = NULL;
    }
  new_node->next = NULL;
  return new_node;
}

// list_insert
[[nodiscard]]
int
list_insert (struct Node **head, void *ptr, const uint64_t amount,
             void **children)
{
  struct Node *new_node = create_node (ptr, amount, children);
  if (!new_node)
    {
      return ERROR_CODE;
    }
  pthread_mutex_lock (&list_mutex);
  if (!*head)
    {
      *head = new_node;
      pthread_mutex_unlock (&list_mutex);
      return SUCCESS_CODE;
    }
  struct Node *temp = *head;
  while (temp->next)
    {
      temp = temp->next;
    }
  temp->next = new_node;
  pthread_mutex_unlock (&list_mutex);
  return SUCCESS_CODE;
}

// free_node
void
free_node (struct Node *current)
{
  if (current->size > 0)
    {
      for (uint64_t i = 0; i < current->size; ++i)
        {
          free (current->children[i]->ptr);
          free (current->children[i]);
        }
      free (current->children);
    }
  free (current->ptr);
  free (current);
}

// list_delete_node
void
list_delete_node (struct Node **head, const void *const ptr)
{
  pthread_mutex_lock (&list_mutex);
  struct Node *current = *head;
  struct Node *prev = NULL;

  if (current && current->ptr == ptr)
    {
      *head = current->next;
      free_node (current);
      pthread_mutex_unlock (&list_mutex);
      return;
    }

  while (current && current->ptr != ptr)
    {
      prev = current;
      current = current->next;
    }

  if (!current)
    {
      pthread_mutex_unlock (&list_mutex);
      return;
    }

  prev->next = current->next;
  free_node (current);
  pthread_mutex_unlock (&list_mutex);
}

// list_free
void
list_free (struct Node **head)
{
  pthread_mutex_lock (&list_mutex);
  struct Node *current = *head;
  struct Node *next;

  while (current)
    {
      next = current->next;
      free_node (current);
      current = next;
    }

  *head = NULL;
  pthread_mutex_unlock (&list_mutex);
}
