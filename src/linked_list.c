#include "linked_list.h"

#include <pthread.h>
#include <stdlib.h>

struct Node
{
  void *ptr;
  size_t size;
  struct Node **children;
  struct Node *next;
};

// Global mutex for thread safety
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

// Función para crear un nuevo nodo
struct Node *
create_node (void *ptr, size_t amount, void **children)
{
  struct Node *new_node = (struct Node *)malloc (sizeof (struct Node));
  if (!new_node)
    {
      return NULL;
    }
  new_node->ptr = ptr;
  new_node->size = amount;
  new_node->children = NULL;
  if (amount > 0)
    {
      new_node->children
          = (struct Node **)malloc (sizeof (struct Node *) * amount);
      if (!new_node->children)
        {
          free (new_node);
          return NULL;
        }
      for (size_t i = 0; i < amount; ++i)
        {
          new_node->children[i] = (struct Node *)malloc (sizeof (struct Node));

          if (!new_node->children[i])
            {
              for (size_t j = 0; j < i; ++j)
                {
                  free (new_node->children[j]);
                }
              free (new_node);
              return NULL;
            }
        }
      for (size_t i = 0; i < amount; ++i)
        {
          new_node->children[i]->size = 0;
          new_node->children[i]->next = NULL;
          new_node->children[i]->ptr = children[i];
          new_node->children[i]->children = NULL;
        }
    }
  new_node->next = NULL;
  return new_node;
}

int
list_insert (struct Node **head, void *ptr, size_t amount, void **children)
{
  struct Node *new_node = create_node (ptr, amount, children);
  if (!new_node)
    {
      return 1;
    }
  pthread_mutex_lock (&list_mutex);
  if (!*head)
    {
      *head = new_node;
      pthread_mutex_unlock (&list_mutex);
      return 0;
    }
  struct Node *temp = *head;
  while (temp->next)
    {
      temp = temp->next;
    }
  temp->next = new_node;
  pthread_mutex_unlock (&list_mutex);
  return 0;
}

void
free_node (struct Node *current)
{
  if (current->size > 0)
    {
      for (size_t i = 0; i < current->size; ++i)
        {
          free (current->children[i]->ptr);
          free (current->children[i]);
        }
      free (current->children);
    }
  free (current->ptr);
  free (current);
}

void
list_delete_node (struct Node **head, void *ptr)
{
  pthread_mutex_lock (&list_mutex);
  struct Node *current = *head;
  struct Node *prev = NULL;

  // Si el nodo a eliminar es el nodo cabeza
  if (current && current->ptr == ptr)
    {
      *head = current->next;
      free_node (current);
      pthread_mutex_unlock (&list_mutex);
      return;
    }

  // Buscar el nodo a eliminar
  while (current && current->ptr != ptr)
    {
      prev = current;
      current = current->next;
    }

  // Si el dato no se encuentra en la lista
  if (!current)
    {
      pthread_mutex_unlock (&list_mutex);
      return;
    }

  // Desenlazar el nodo y liberarlo
  prev->next = current->next;
  free_node (current);
  pthread_mutex_unlock (&list_mutex);
}

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
