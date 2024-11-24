#include "threadpool.h"
#include "config.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define TRUE 1
#define FALSE 0

struct task
{
  void (*function) (void *);
  void *argument;
  struct task *next;
};

struct threadpool
{
  pthread_t *threads;           // array of threads
  size_t num_threads;           // number of threads
  struct task *task_queue_head; // head of task queue
  struct task *task_queue_tail; // tail of task queue
  pthread_mutex_t queue_lock;   // mutex for the task queue
  pthread_cond_t queue_cond;    // condition variable for tasks
  pthread_cond_t empty_cond;    // condition variable for empty queue
  int stop;                     // flag to indicate if threadpool is stopping
  int started;                  // flag to indicate if threadpool has started
};

struct threadpool *
threadpool_create (size_t num_threads)
{
  LOG_INIT (" ");
  struct threadpool *pool = malloc (sizeof (struct threadpool));
  if (pool == NULL)
    {
      perror ("Failed to allocate threadpool");
      LOG_END (" ");
      return NULL;
    }
  if (num_threads < 3)
    {
      num_threads = 3;
    }
  if (num_threads > NUM_CORES)
    {
      num_threads = NUM_CORES;
    }

  pool->num_threads = num_threads;
  pool->threads = malloc (sizeof (pthread_t) * num_threads);
  if (pool->threads == NULL)
    {
      perror ("Failed to allocate threads array");
      free (pool);
      LOG_END (" ");
      return NULL;
    }

  pool->task_queue_head = NULL;
  pool->task_queue_tail = NULL;
  pool->stop = FALSE;
  pool->started = FALSE;

  if (pthread_mutex_init (&pool->queue_lock, NULL) != 0)
    {
      perror ("Failed to initialize mutex");
      free (pool->threads);
      free (pool);
      LOG_END (" ");
      return NULL;
    }

  if (pthread_cond_init (&pool->queue_cond, NULL) != 0)
    {
      perror ("Failed to initialize condition variable");
      pthread_mutex_destroy (&pool->queue_lock);
      free (pool->threads);
      free (pool);
      LOG_END (" ");
      return NULL;
    }

  if (pthread_cond_init (&pool->empty_cond, NULL) != 0)
    {
      perror ("Failed to initialize empty condition variable");
      pthread_mutex_destroy (&pool->queue_lock);
      pthread_cond_destroy (&pool->queue_cond);
      free (pool->threads);
      free (pool);
      LOG_END (" ");
      return NULL;
    }

  LOG_END (" ");
  return pool;
}

struct threadpool *
threadpool_create_default (void)
{
  return threadpool_create (NUM_CORES);
}

void *
threadpool_worker (void *arg)
{
  LOG_INIT (" ");
  struct threadpool *pool = (struct threadpool *)arg;
  while (TRUE)
    {
      pthread_mutex_lock (&pool->queue_lock);
      while (pool->task_queue_head == NULL && !pool->stop)
        {
          pthread_cond_wait (&pool->queue_cond, &pool->queue_lock);
        }

      if (pool->stop && pool->task_queue_head == NULL)
        {
          pthread_mutex_unlock (&pool->queue_lock);
          break;
        }

      struct task *task = pool->task_queue_head;
      if (task != NULL)
        {
          // Remove task from queue
          pool->task_queue_head = task->next;
          if (pool->task_queue_head == NULL)
            pool->task_queue_tail = NULL;

          // If queue is empty, signal empty_cond
          if (pool->task_queue_head == NULL)
            {
              pthread_cond_signal (&pool->empty_cond);
            }
        }
      pthread_mutex_unlock (&pool->queue_lock);

      if (task != NULL)
        {
          // Execute task
          task->function (task->argument);
          free (task);
        }
    }
  LOG_END (" ");
  pthread_exit (NULL);
  return NULL;
}

void
threadpool_init (struct threadpool *pool)
{
  LOG_INIT (" ");
  if (pool == NULL || pool->started)
    {
      // Already started or invalid pool
      LOG_END (" ");
      return;
    }
  for (size_t i = 0; i < pool->num_threads; i++)
    {
      if (pthread_create (&pool->threads[i], NULL, threadpool_worker,
                          (void *)pool)
          != 0)
        {
          perror ("Failed to create thread");
          pool->stop = TRUE;
          break;
        }
    }
  pool->started = TRUE;
  LOG_END (" ");
}

void
threadpool_add (struct threadpool *pool, void (*function) (void *),
                void *argument)
{
  LOG_INIT (" ");
  if (pool == NULL || function == NULL)
    {
      LOG_END (" ");
      return;
    }

  struct task *new_task = malloc (sizeof (struct task));
  if (new_task == NULL)
    {
      perror ("Failed to allocate task");
      LOG_END (" ");
      return;
    }

  new_task->function = function;
  new_task->argument = argument;
  new_task->next = NULL;

  pthread_mutex_lock (&pool->queue_lock);

  if (pool->task_queue_head == NULL)
    {
      pool->task_queue_head = new_task;
      pool->task_queue_tail = new_task;
      // Signal worker threads that a new task is available
    }
  else
    {
      pool->task_queue_tail->next = new_task;
      pool->task_queue_tail = new_task;
    }
  pthread_cond_signal (&pool->queue_cond);

  pthread_mutex_unlock (&pool->queue_lock);
  LOG_END (" ");
}

void
threadpool_stop (struct threadpool *pool)
{
  LOG_INIT (" ");
  if (pool == NULL || !pool->started)
    {
      LOG_END (" ");
      return;
    }
  threadpool_wait_empty (pool);

  pthread_mutex_lock (&pool->queue_lock);
  pool->stop = TRUE;
  pthread_cond_broadcast (&pool->queue_cond);
  pthread_mutex_unlock (&pool->queue_lock);

  for (size_t i = 0; i < pool->num_threads; i++)
    {
      pthread_join (pool->threads[i], NULL);
    }
  pool->started = FALSE;
  LOG_END (" ");
}

int
threadpool_empty (struct threadpool *pool)
{
  LOG_INIT (" ");
  if (pool == NULL)
    {
      LOG_END (" ");
      return TRUE;
    }
  pthread_mutex_lock (&pool->queue_lock);
  int empty = (pool->task_queue_head == NULL);
  pthread_mutex_unlock (&pool->queue_lock);
  LOG_END (" ");
  return empty;
}

void
threadpool_wait_empty (struct threadpool *pool)
{
  LOG_INIT (" ");
  if (pool == NULL)
    {
      LOG_END (" ");
      return;
    }
  pthread_mutex_lock (&pool->queue_lock);
  while (pool->task_queue_head != NULL)
    {
      pthread_cond_wait (&pool->empty_cond, &pool->queue_lock);
    }
  pthread_mutex_unlock (&pool->queue_lock);
  LOG_END (" ");
}

void
threadpool_destroy (struct threadpool *pool)
{
  LOG_INIT (" ");
  if (pool == NULL)
    {
      LOG_END (" ");
      return;
    }
  threadpool_stop (pool);

  // Clean up remaining tasks
  pthread_mutex_lock (&pool->queue_lock);
  struct task *task = pool->task_queue_head;
  while (task != NULL)
    {
      struct task *tmp = task;
      task = task->next;
      free (tmp);
    }
  pthread_mutex_unlock (&pool->queue_lock);

  pthread_mutex_destroy (&pool->queue_lock);
  pthread_cond_destroy (&pool->queue_cond);
  pthread_cond_destroy (&pool->empty_cond);

  free (pool->threads);
  free (pool);
  LOG_END (" ");
}
