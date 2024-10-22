/*!
 * @defgroup ThreadPool
 *
 * @{
 */
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  struct task;

  struct threadpool;

  struct threadpool *threadpool_create (size_t num_threads);

  struct threadpool *threadpool_create_default (void);

  void threadpool_init (struct threadpool *pool);

  void threadpool_add (struct threadpool *pool, void (*function) (void *),
                       void *argument);

  void threadpool_stop (struct threadpool *pool);

  int threadpool_empty (struct threadpool *pool);

  void threadpool_wait_empty (struct threadpool *pool);

  void threadpool_destroy (struct threadpool *pool);

#ifdef __cplusplus
}
#endif

#endif // !THREADPOOL_H

/*!
 * @}
 */
