/*!
 * @defgroup ThreadPool
 *
 * @brief A thread pool implementation for managing and executing tasks.
 *
 * This module provides functionality to manage a pool of threads that execute
 * tasks in a synchronized manner.
 *
 * ### Thread Pool Overview:
 *
 * ```
 * Threads in pool: [T1] [T2] [T3] ... [Tn]
 * Task Queue:
 * [Task(function=A, argument=X)] -> [Task(function=B, argument=Y)] -> NULL
 * ```
 *
 * ### Example Usage:
 *
 * ```c
 * #include "threadpool.h"
 * #include <stdio.h>
 *
 * void print_task(void *arg) {
 *     int *val = (int *)arg;
 *     printf("Task executed with value: %d\n", *val);
 * }
 *
 * int main() {
 *     struct threadpool *pool = threadpool_create_default();
 *     threadpool_init(pool);
 *
 *     int data1 = 42, data2 = 24;
 *
 *     threadpool_add(pool, print_task, &data1);
 *     threadpool_add(pool, print_task, &data2);
 *
 *     threadpool_wait_empty(pool);
 *     threadpool_destroy(pool);
 *     return 0;
 * }
 * ```
 *
 * @author Israel
 * @date 2024
 *
 * @{
 */
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdint.h> // for uint64_t

#ifdef __cplusplus
extern "C"
{
#endif

  /*!
   * @struct threadpool
   * @brief Represents a thread pool.
   *
   * The thread pool manages a fixed number of worker threads and a queue of
   * tasks.
   */
  struct threadpool;

  /*!
   * @brief Creates a new thread pool with the specified number of threads.
   *
   * @param num_threads The number of threads in the pool.
   * @return A pointer to the created thread pool, or `NULL` if creation fails.
   *
   * ### Diagram:
   * ```
   * Input:
   * num_threads = 4
   *
   * Output:
   * ThreadPool:
   *   - Threads: [T1, T2, T3, T4]
   *   - Task Queue: Empty
   * ```
   */
  struct threadpool *threadpool_create (uint64_t num_threads);

  /*!
   * @brief Creates a new thread pool with the default number of threads (equal
   * to the number of CPU cores).
   *
   * @return A pointer to the created thread pool, or `NULL` if creation fails.
   */
  struct threadpool *threadpool_create_default (void);

  /*!
   * @brief Initializes the thread pool, starting the worker threads.
   *
   * @param pool Pointer to the thread pool to initialize.
   */
  void threadpool_init (struct threadpool *pool);

  /*!
   * @brief Adds a task to the thread pool.
   *
   * @param pool Pointer to the thread pool.
   * @param function Pointer to the function representing the task.
   * @param argument Pointer to the argument to pass to the task function.
   *
   * ### Diagram:
   * ```
   * Before:
   * Task Queue: Empty
   *
   * After:
   * Task Queue: [Task(function=A, argument=X)] -> NULL
   * ```
   */
  void threadpool_add (struct threadpool *pool, void (*function) (void *),
                       void *argument);

  /*!
   * @brief Stops all threads in the thread pool and prevents further tasks
   * from being added.
   *
   * @param pool Pointer to the thread pool to stop.
   *
   * ### Diagram:
   * ```
   * Before:
   * Threads: [Running T1, Running T2]
   * Task Queue: [Task1] -> [Task2] -> NULL
   *
   * After:
   * Threads: Stopped
   * Task Queue: Empty
   * ```
   */
  void threadpool_stop (struct threadpool *pool);

  /*!
   * @brief Checks if the thread pool's task queue is empty.
   *
   * @param pool Pointer to the thread pool.
   * @return `1` if the task queue is empty, `0` otherwise.
   */
  int threadpool_empty (struct threadpool *pool);

  /*!
   * @brief Waits until the task queue becomes empty.
   *
   * @param pool Pointer to the thread pool.
   */
  void threadpool_wait_empty (struct threadpool *pool);

  /*!
   * @brief Destroys the thread pool, freeing all allocated resources.
   *
   * @param pool Pointer to the thread pool to destroy.
   *
   * ### Diagram:
   * ```
   * Before:
   * ThreadPool:
   *   - Threads: [T1, T2, T3]
   *   - Task Queue: [Task1] -> [Task2] -> NULL
   *
   * After:
   * ThreadPool: Destroyed
   * ```
   */
  void threadpool_destroy (struct threadpool *pool);

#ifdef __cplusplus
}
#endif

#endif // !THREADPOOL_H

/*!
 * @}
 */
