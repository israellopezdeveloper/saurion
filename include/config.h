#ifndef CONFIG_H
#define CONFIG_H

#include <unistd.h>

#define NUM_CORES sysconf(_SC_NPROCESSORS_ONLN)

/*! @brief Size of iovec */
#define CHUNK_SZ 8192
#define ACCEPT_QUEUE 10
#define SAURION_RING_SIZE 256
#define TIMEOUT_RETRY 10
#define MAX_ATTEMPTS 10

/*! @brief Something goes wrong */
#define ERROR_CODE 0
/*! @brief Everything goes well */
#define SUCCESS_CODE 1

#endif  // !CONFIG_H
