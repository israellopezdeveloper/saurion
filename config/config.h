#ifndef CONFIG_H
#define CONFIG_H

#include <unistd.h>

#define NUM_CORES sysconf(_SC_NPROCESSORS_ONLN)

#define CHUNK_SZ 8192  //! @brief Tamaño del buffer de lectura.
#define ACCEPT_QUEUE 10
#define SAURION_RING_SIZE 256
#define TIMEOUT_RETRY 10
#define MAX_ATTEMPTS 10

#define ERROR_CODE 0
#define SUCCESS_CODE 1

#define DEBUG 0

#include "logger.h"

#endif  // !CONFIG_H
