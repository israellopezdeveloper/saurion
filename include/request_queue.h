/*!
 * @defgroup RequestQueue
 *
 * @brief This module provides a thread-safe queue for managing requests.
 *
 * Diagram:
 * @verbatim
  +---------------------+
  |   request_queue     |
  +---------+-----------+
            |
   +--------v---------+       +---------+       +---------+
   |    queue_node    | ----> | request | ----> |  next    |
   +------------------+       +---------+       +---------+
 @endverbatim
 *
 * Example usage:
 * @code
 * struct request_queue queue;
 * struct request req;
 *
 * if (init_queue(&queue) != SUCCESS_CODE) {
 * }
 *
 * enqueue(&queue, &req);
 * struct request *processed_req = dequeue(&queue);
 *
 * destroy_queue(&queue);
 *
 * @endcode
 *
 * @author Israel
 * @date 2024
 *
 * @{
 */
#ifndef REQUEST_QUEUE_H
#define REQUEST_QUEUE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <pthread.h> // for pthread_mutex_t, pthread_cond_t

  /*!
   * @cond
   * This is hidden from the documentation.
   */
  struct request;
  /*! @endcond */

  /*!
   * @brief Represents a node in the request queue.
   *
   * Each node contains a pointer to a `request` and a pointer to the next node
   * in the queue.
   * @private
   */
  struct queue_node
  {
    struct request *req;     /*!< Pointer to the request. @private */
    struct queue_node *next; /*!< Pointer to the next node. @private */
  };

  /*!
   * @brief Represents a thread-safe queue for managing requests.
   *
   * The queue is implemented as a linked list with pointers to the front and
   * rear nodes, and is synchronized using a mutex and a condition variable.
   * @private
   */
  struct request_queue
  {
    struct queue_node *front; /*!< Front of the queue. */
    struct queue_node *rear;  /*!< Rear of the queue. */
    pthread_mutex_t lock;     /*!< Mutex for thread-safety. @private */
    pthread_cond_t cond;      /*!< Condition variable for synchronization.
                        @private */
  };

  /*!
   * @brief Initializes the request queue.
   *
   * @param[in,out] queue Pointer to the request queue to initialize.
   * @return SUCCESS_CODE on success, ERROR_CODE on failure.
   */
  [[nodiscard]] int init_queue (struct request_queue *queue);

  /*!
   * @brief Enqueues a request into the queue.
   *
   * @param[in,out] queue Pointer to the queue where the request will be added.
   * @param[in] req Pointer to the request to be enqueued.
   * @return SUCCESS_CODE on success, ERROR_CODE on failure.
   */
  [[nodiscard]] int enqueue (struct request_queue *queue, struct request *req);

  /*!
   * @brief Dequeues a request from the queue.
   *
   * @param[in,out] queue Pointer to the queue from which to dequeue.
   * @return Pointer to the dequeued request, or NULL if the queue is empty.
   */
  [[nodiscard]] struct request *dequeue (struct request_queue *queue);

  /*!
   * @brief Destroys the request queue, freeing all allocated resources.
   *
   * @param[in,out] queue Pointer to the queue to destroy.
   */
  void destroy_queue (struct request_queue *queue);

#ifdef __cplusplus
}
#endif

#endif // !REQUEST_QUEUE_H

/*!
 * @}
 */
