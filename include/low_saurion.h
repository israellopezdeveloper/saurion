/*!
 * @defgroup LowSaurion
 *
 * @brief The `saurion` class is designed to efficiently handle asynchronous
 * input/output events on Linux systems using the `io_uring` API. Its main
 * purpose is to manage network operations such as socket connections, reads,
 * writes, and closures by leveraging an event-driven model that enhances
 * performance and scalability in highly concurrent applications.
 *
 * The main structure, `saurion`, encapsulates `io_uring` rings and facilitates
 * synchronization between multiple threads through the use of mutexes and a
 * thread pool that distributes operations in parallel. This allows efficient
 * handling of I/O operations across several sockets simultaneously, without
 * blocking threads during operations.
 *
 * The messages are composed of three main parts:
 *  - A header, which is an unsigned 64-bit number representing the length of
 * the message body.
 *  - A body, which contains the actual message data.
 *  - A footer, which consists of 8 bits set to 0.
 *
 * For example, for a message with 9000 bytes of content, the header would
 * contain the number 9000, the body would consist of those 9000 bytes, and the
 * footer would be 1 byte set to 0.
 *
 * When these messages are sent to the kernel, they are divided into chunks
 * using `iovec`. Each chunk can hold a maximum of 8192 bytes and contains two
 * fields:
 *  - `iov_base`, which is an array where the chunk of the message is stored.
 *  - `iov_len`, the number of bytes used in the `iov_base` array.
 *
 * For the message with 9000 bytes, the `iovec` division would look like this:
 *
 * - The first `iovec` would contain:
 *   - 8 bytes for the header (the length of the message body, 9000).
 *   - 8184 bytes of the message body.
 *   - `iov_len` would be 8192 bytes in total.
 *
 * - The second `iovec` would contain:
 *   - The remaining 816 bytes of the message body.
 *   - 1 byte for the footer (set to 0).
 *   - `iov_len` would be 817 bytes in total.
 *
 * The structure of the message is as follows:
 * @verbatim
   +------------------+--------------------+----------+
   |    Header        |       Body         |  Footer  |
   |  (64 bits: 9000) |   (Message Data)   | (1 byte) |
   +------------------+--------------------+----------+
 @endverbatim
 *
 * The structure of the `iovec` division is:
 *
 * @verbatim
   First iovec (8192 bytes):
   +-----------------------------------------+-----------------------+
   | iov_base                                | iov_len               |
   +-----------------------------------------+-----------------------+
   | 8 bytes header, 8184 bytes of message   | 8192                  |
   +-----------------------------------------+-----------------------+

   Second iovec (817 bytes):
   +-----------------------------------------+-----------------------+
   | iov_base                                | iov_len               |
   +-----------------------------------------+-----------------------+
   | 816 bytes of message, 1 byte footer (0) | 817                   |
   +-----------------------------------------+-----------------------+
 @endverbatim
 *
 * Each I/O event can be monitored and managed through custom callbacks that
 * handle connection, read, write, close, or error events on the sockets.
 *
 * Basic usage example:
 *
 * @code
 * // Create the saurion structure with 4 threads
 * struct saurion *s = saurion_create(4);
 *
 * // Start event processing
 * if (saurion_start(s) != 0) {
 *     // Handle the error
 * }
 *
 * // Send a message through a socket
 * saurion_send(s, socket_fd, "Hello, World!");
 *
 * // Stop event processing
 * saurion_stop(s);
 *
 * // Destroy the structure and free resources
 * saurion_destroy(s);
 * @endcode
 *
 * In this example, the `saurion` structure is created with 4 threads to handle
 * the workload. Event processing is started, allowing it to accept connections
 * and manage I/O operations on sockets. After sending a message through a
 * socket, the system can be stopped, and the resources are freed.
 *
 * @author Israel
 * @date 2024
 *
 * @{
 */
#ifndef LOW_SAURION_H
#define LOW_SAURION_H

#define _POSIX_C_SOURCE 200809L

#include <pthread.h>   // for pthread_mutex_t, pthread_cond_t
#include <stdint.h>    // for uint32_t
#include <sys/types.h> // for ssize_t

#ifdef __cplusplus
extern "C"
{
#endif

/*!
 * @brief Defines the memory alignment size for structures in the `saurion`
 * class.
 *
 * `PACKING_SZ` is used to ensure that certain structures, such as
 * `saurion_callbacks`, are aligned to a specific memory boundary. This can
 * improve memory access performance and ensure compatibility with certain
 * hardware architectures that require specific alignment.
 *
 * In this case, the value is set to 32 bytes, meaning that structures marked
 * with
 * `__attribute__((aligned(PACKING_SZ)))` will be aligned to 32-byte
 * boundaries.
 *
 * Proper alignment can be particularly important in multithreaded environments
 * or when working with low-level system APIs like `io_uring`, where unaligned
 * memory accesses may introduce performance penalties.
 *
 * Adjusting `PACKING_SZ` may be necessary depending on the hardware platform
 * or specific performance requirements.
 */
#define PACKING_SZ 32

  /*!
   * @brief Main structure for managing io_uring and socket events.
   *
   * This structure contains all the necessary data to handle the io_uring
   * event queue and the callbacks for socket events, enabling efficient
   * asynchronous I/O operations.
   */
  struct saurion
  {
    /*! Array of io_uring structures for managing the event queue. */
    struct io_uring *rings;
    /*! Array of mutexes to protect the io_uring rings. */
    pthread_mutex_t *m_rings;
    /*! Server socket descriptor for accepting connections. */
    int ss;
    /*! Eventfd descriptors used for internal signaling between threads. */
    int *efds;
    /*! Linked list for storing active requests. */
    struct Node *list;
    /*! Mutex to protect the state of the structure. */
    pthread_mutex_t status_m;
    /*! Condition variable to signal changes in the structure's state. */
    pthread_cond_t status_c;
    /*! Current status of the structure (e.g., running, stopped). */
    int status;
    /*! Thread pool for executing tasks in parallel. */
    struct threadpool *pool;
    /*! Number of threads in the thread pool. */
    uint32_t n_threads;
    /*! Index of the next io_uring ring to which an event will be added. */
    uint32_t next;

    /*!
     * @brief Structure containing callback functions to handle socket events.
     *
     * This structure holds pointers to callback functions for handling events
     * such as connection establishment, reading, writing, closing, and errors
     * on sockets. Each callback has an associated argument pointer that can be
     * passed along when the callback is invoked.
     */
    struct saurion_callbacks
    {
      /*!
       * @brief Callback for handling new connections.
       *
       * @param fd File descriptor of the connected socket.
       * @param arg Additional user-provided argument.
       */
      void (*on_connected) (const int fd, void *arg);
      /*! Additional argument for the connection callback. */
      void *on_connected_arg;

      /*!
       * @brief Callback for handling read events.
       *
       * @param fd File descriptor of the socket.
       * @param content Pointer to the data that was read.
       * @param len Length of the data that was read.
       * @param arg Additional user-provided argument.
       */
      void (*on_readed) (const int fd, const void *const content,
                         const ssize_t len, void *arg);
      /*! Additional argument for the read callback. */
      void *on_readed_arg;

      /*!
       * @brief Callback for handling write events.
       *
       * @param fd File descriptor of the socket.
       * @param arg Additional user-provided argument.
       */
      void (*on_wrote) (const int fd, void *arg);
      void *on_wrote_arg; /**< Additional argument for the write callback. */

      /*!
       * @brief Callback for handling socket closures.
       *
       * @param fd File descriptor of the closed socket.
       * @param arg Additional user-provided argument.
       */
      void (*on_closed) (const int fd, void *arg);
      /*! Additional argument for the close callback. */
      void *on_closed_arg;

      /*!
       * @brief Callback for handling error events.
       *
       * @param fd File descriptor of the socket where the error occurred.
       * @param content Pointer to the error message.
       * @param len Length of the error message.
       * @param arg Additional user-provided argument.
       */
      void (*on_error) (const int fd, const char *const content,
                        const ssize_t len, void *arg);
      /*! Additional argument for the error callback. */
      void *on_error_arg;
    } __attribute__ ((aligned (PACKING_SZ))) cb;
  } __attribute__ ((aligned (PACKING_SZ)));

  /*!
   *  @todo Eliminar
   */
  int EXTERNAL_set_socket (int p);

  /*!
   * @public
   * @brief Creates an instance of the `saurion` structure.
   *
   * This function initializes the `saurion` structure, sets up the eventfd,
   * and configures the io_uring queue, preparing it for use. It also sets up
   * the thread pool and any necessary synchronization mechanisms.
   *
   * @param n_threads The number of threads to initialize in the thread pool.
   * @return struct saurion* A pointer to the newly created `saurion`
   * structure, or NULL if an error occurs.
   */
  [[nodiscard]]
  struct saurion *saurion_create (uint32_t n_threads);

  /*!
   * @public
   * @brief Starts event processing in the `saurion` structure.
   *
   * This function begins accepting socket connections and handling io_uring
   * events in a loop. It will run continuously until a stop signal is
   * received, allowing the application to manage multiple socket events
   * asynchronously.
   *
   * @param s Pointer to the `saurion` structure.
   * @return int Returns 0 on success, or 1 if an error occurs.
   */
  [[nodiscard]]
  int saurion_start (struct saurion *s);

  /*!
   * @public
   * @brief Stops event processing in the `saurion` structure.
   *
   * This function sends a signal to the eventfd, indicating that the event
   * loop should stop. It gracefully shuts down the processing of any remaining
   * events before exiting.
   *
   * @param s Pointer to the `saurion` structure.
   */
  void saurion_stop (const struct saurion *s);

  /*!
   * @public
   * @brief Destroys the `saurion` structure and frees all associated
   * resources.
   *
   * This function waits for the event processing to stop, frees the memory
   * used by the `saurion` structure, and closes any open file descriptors. It
   * ensures that no resources are leaked when the structure is no longer
   * needed.
   *
   * @param s Pointer to the `saurion` structure.
   */
  void saurion_destroy (struct saurion *s);

  /*!
   * @public
   * @brief Sends a message through a socket using io_uring.
   *
   * This function prepares and sends a message through the specified socket
   * using the io_uring event queue. The message is split into iovec structures
   * for efficient transmission and sent asynchronously.
   *
   * @param s Pointer to the `saurion` structure.
   * @param fd File descriptor of the socket to which the message will be sent.
   * @param msg Pointer to the character string (message) to be sent.
   */
  void saurion_send (struct saurion *s, const int fd, const char *const msg);

#ifdef __cplusplus
}
#endif

#endif // !LOW_SAURION_H

/*!
 * @}
 */
