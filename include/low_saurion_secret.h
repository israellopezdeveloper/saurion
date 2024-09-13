#ifndef LOW_SAURION_SECRET_H
#define LOW_SAURION_SECRET_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief This function allocates memory for each `struct iovec`
 *
 * This function allocates memory for each `struct iovec`. Every `struct iovec`
 * consists of two member variables:
 *   - `iov_base`, a `void *` array that will hold the data. All of them will
 *     allocate the same amount of memory (CHUNK_SZ) to avoid memory
 * fragmentation.
 *   - `iov_len`, an integer representing the size of the data stored in the
 *     `iovec`. The data size is CHUNK_SZ unless it's the last one, in which
 *     case it will hold the remaining bytes. In addition to initialization,
 *     the function adds the pointers to the allocated memory into a child array
 *     to simplify memory deallocation later on.
 *
 * @param iov Structure to initialize.
 * @param amount Total number of `iovec` to initialize.
 * @param pos Current position of the `iovec` within the total `iovec` (\p amount).
 * @param size Total size of the data to be stored in the `iovec`.
 * @param chd_ptr Array to hold the pointers to the allocated memory.
 *
 * @retval ERROR_CODE if there was an error during memory allocation.
 * @retval SUCCESS_CODE if the operation was successful.
 *
 * @note The last `iovec` will allocate only the remaining bytes if the total
 * size is not a multiple of CHUNK_SZ.
 */
[[nodiscard]]
int allocate_iovec(struct iovec *iov, size_t amount, size_t pos, size_t size, void **chd_ptr);

/*!
 * @brief Initializes a specified `iovec` structure with a message fragment.
 *
 * This function populates the `iov_base` of the `iovec` structure with a portion
 * of the message, depending on the position (`pos`) in the overall set of iovec structures.
 * The message is divided into chunks, and for the first `iovec`, a header containing
 * the size of the message is included. Optionally, padding or adjustments can be applied
 * based on the `h` flag.
 *
 * @param iov Pointer to the `iovec` structure to initialize.
 * @param amount The total number of `iovec` structures.
 * @param pos The current position of the `iovec` within the overall message split.
 * @param msg Pointer to the message to be split across the `iovec` structures.
 * @param size The total size of the message.
 * @param h A flag (header flag) that indicates whether special handling is needed for
 *          the first `iovec` (adds the message size as a header) or for the last chunk.
 *
 * @retval SUCCESS_CODE on successful initialization of the `iovec`.
 * @retval ERROR_CODE if the `iov` or its `iov_base` is null.
 *
 * @note For the first `iovec` (when `pos == 0`), the message size is copied into the
 *       beginning of the `iov_base` if the header flag (`h`) is set. Subsequent chunks
 *       are filled with message data, and the last chunk may have one byte reduced if
 *       `h` is set.
 *
 * @attention The message must be properly aligned and divided, especially when using
 *            the header flag to ensure no memory access issues.
 *
 * @warning If `msg` is null, the function will initialize the `iov_base` with zeros,
 *          essentially resetting the buffer.
 */
[[nodiscard]]
int initialize_iovec(struct iovec *iov, size_t amount, size_t pos, void *msg, size_t size,
                     uint8_t h);

#ifdef __cplusplus
}
#endif

#endif  // !LOW_SAURION_SECRET_H
