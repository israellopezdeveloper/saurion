#include "request_queue.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h> // for malloc, free

// init_queue
[[nodiscard]]
int
init_queue (struct request_queue *queue)
{
  queue->front = NULL;
  queue->rear = NULL;
  if (pthread_mutex_init (&queue->lock, NULL)
      || pthread_cond_init (&queue->cond, NULL))
    {
      return ERROR_CODE;
    }
  return SUCCESS_CODE;
}

// enqueue
[[nodiscard]]
int
enqueue (struct request_queue *queue, struct request *req)
{
  struct queue_node *new_node
      = (struct queue_node *)malloc (sizeof (struct queue_node));
  if (!new_node)
    {
      return ERROR_CODE;
    }
  new_node->req = req;
  new_node->next = NULL;

  pthread_mutex_lock (&queue->lock);

  if (!queue->rear)
    {
      queue->front = new_node;
      queue->rear = new_node;
    }
  else
    {
      queue->rear->next = new_node;
      queue->rear = new_node;
    }

  pthread_cond_signal (&queue->cond);
  pthread_mutex_unlock (&queue->lock);
  return SUCCESS_CODE;
}

// dequeue
[[nodiscard]]
struct request *
dequeue (struct request_queue *queue)
{
  pthread_mutex_lock (&queue->lock);

  while (queue->front == NULL)
    {
      pthread_cond_wait (&queue->cond, &queue->lock);
    }

  struct queue_node *temp = queue->front;
  struct request *req = temp->req;

  queue->front = queue->front->next;
  if (queue->front == NULL)
    {
      queue->rear = NULL;
    }

  free (temp);
  pthread_mutex_unlock (&queue->lock);

  return req;
}

// destroy_queue
void
destroy_queue (struct request_queue *queue)
{
  pthread_mutex_lock (&queue->lock);

  struct queue_node *current = queue->front;
  while (current)
    {
      struct queue_node *next = current->next;
      free (current->req);
      free (current);
      current = next;
    }
  queue->front = NULL;
  queue->rear = NULL;

  pthread_mutex_unlock (&queue->lock);

  pthread_mutex_destroy (&queue->lock);
  pthread_cond_destroy (&queue->cond);
}
