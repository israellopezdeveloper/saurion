#ifndef LOGGER_H
#define LOGGER_H

#define CRITICAL(...) \
  {}

#define ERROR(...) \
  {}

#define WARN(...) \
  {}

#define LOG(...) \
  {}

#define LOG_INIT(...) \
  {}

#define LOG_END(...) \
  {}

#ifdef DEBUG

#include <execinfo.h>
#include <libgen.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CRITICAL(fmt, ...)                                                                  \
  {                                                                                         \
    struct timespec timespec {};                                                            \
    clock_gettime(CLOCK_MONOTONIC, &timespec);                                              \
    void **buffer = (void **)malloc(50 * sizeof(void *));                                   \
    if (!buffer) return;                                                                    \
    int deep = backtrace(buffer, 50);                                                       \
    void **deep_ptr = buffer;                                                               \
    char *parents = (char *)malloc(static_cast<size_t>(deep * 15) * sizeof(char));          \
    if (!parents) {                                                                         \
      free(buffer);                                                                         \
      return;                                                                               \
    }                                                                                       \
    char *parents_ptr = parents;                                                            \
    for (int i = 0; i < deep; i++) {                                                        \
      if ((i > 0) && (i < deep)) {                                                          \
        *parents_ptr = ',';                                                                 \
        ++parents_ptr;                                                                      \
      }                                                                                     \
      sprintf(parents_ptr, "%p", *deep_ptr);                                                \
      parents_ptr += 14;                                                                    \
      deep_ptr++;                                                                           \
    }                                                                                       \
    flockfile(stderr);                                                                      \
    fprintf(stderr,                                                                         \
            "< 💣 > th_id=%zu time=%zu |> file=%-25s line=%-4d deep=%-2d function=%-30s " \
            "function_id=%-14p parents=<%s> |> position=punctual |> " fmt "\n",             \
            pthread_self() % 1000, (timespec.tv_sec * 1000000) + (timespec.tv_nsec / 1000), \
            basename(__FILE__), __LINE__, deep, __func__, *(buffer + 2),                    \
            parents __VA_OPT__(, ) __VA_ARGS__);                                            \
    funlockfile(stderr);                                                                    \
    free(buffer);                                                                           \
    free(parents);                                                                          \
  }

#if DEBUG > 0

#define ERROR(fmt, ...)                                                                         \
  {                                                                                             \
    struct timespec timespec;                                                                   \
    clock_gettime(CLOCK_MONOTONIC, &timespec);                                                  \
    void **buffer = (void **)malloc(50 * sizeof(void *));                                       \
    if (buffer) {                                                                               \
      int deep = backtrace(buffer, 50);                                                         \
      void **deep_ptr = buffer;                                                                 \
      char *parents = (char *)malloc(static_cast<size_t>(deep * 15) * sizeof(char));            \
      if (parents) {                                                                            \
        char *parents_ptr = parents;                                                            \
        for (int i = 0; i < deep; i++) {                                                        \
          if ((i > 0) && (i < deep)) {                                                          \
            *parents_ptr = ',';                                                                 \
            ++parents_ptr;                                                                      \
          }                                                                                     \
          sprintf(parents_ptr, "%p", *deep_ptr);                                                \
          parents_ptr += 14;                                                                    \
          deep_ptr++;                                                                           \
        }                                                                                       \
        flockfile(stderr);                                                                      \
        fprintf(stderr,                                                                         \
                "< ⏰ > th_id=%zu time=%zu |> file=%-25s line=%-4d deep=%-2d function=%-30s "  \
                "function_id=%-14p parents=<%s> |> position=punctual |> " fmt "\n",             \
                pthread_self() % 1000, (timespec.tv_sec * 1000000) + (timespec.tv_nsec / 1000), \
                basename(__FILE__), __LINE__, deep, __func__, *(buffer + 2),                    \
                parents __VA_OPT__(, ) __VA_ARGS__);                                            \
        funlockfile(stderr);                                                                    \
        free(buffer);                                                                           \
        free(parents);                                                                          \
      } else {                                                                                  \
        free(buffer);                                                                           \
      }                                                                                         \
    }                                                                                           \
  }

#if DEBUG > 1

#define WARN(fmt, ...)                                                                          \
  {                                                                                             \
    struct timespec timespec;                                                                   \
    clock_gettime(CLOCK_MONOTONIC, &timespec);                                                  \
    void **buffer = (void **)malloc(50 * sizeof(void *));                                       \
    if (buffer) {                                                                               \
      int deep = backtrace(buffer, 50);                                                         \
      void **deep_ptr = buffer;                                                                 \
      char *parents = (char *)malloc((int64_t)(deep * 15) * sizeof(char));                      \
      if (parents) {                                                                            \
        char *parents_ptr = parents;                                                            \
        for (int i = 0; i < deep; i++) {                                                        \
          if ((i > 0) && (i < deep)) {                                                          \
            *parents_ptr = ',';                                                                 \
            ++parents_ptr;                                                                      \
          }                                                                                     \
          sprintf(parents_ptr, "%p", *deep_ptr);                                                \
          parents_ptr += 14;                                                                    \
          deep_ptr++;                                                                           \
        }                                                                                       \
        flockfile(stderr);                                                                      \
        fprintf(stderr,                                                                         \
                "< ❗ > th_id=%zu time=%zu |> file=%-25s line=%-4d deep=%-2d function=%-30s "  \
                "function_id=%-14p parents=<%s> |> position=punctual |> " fmt "\n",             \
                pthread_self() % 1000, (timespec.tv_sec * 1000000) + (timespec.tv_nsec / 1000), \
                basename(__FILE__), __LINE__, deep, __func__, *(buffer + 2),                    \
                parents __VA_OPT__(, ) __VA_ARGS__);                                            \
        funlockfile(stderr);                                                                    \
        free(buffer);                                                                           \
        free(parents);                                                                          \
      } else {                                                                                  \
        free(buffer);                                                                           \
      }                                                                                         \
    }                                                                                           \
  }

#if DEBUG > 2

#define LOG_INIT(fmt, ...)                                                                      \
  {                                                                                             \
    struct timespec timespec;                                                                   \
    clock_gettime(CLOCK_MONOTONIC, &timespec);                                                  \
    void **buffer = (void **)malloc(50 * sizeof(void *));                                       \
    if (buffer) {                                                                               \
      int deep = backtrace(buffer, 50);                                                         \
      void **deep_ptr = buffer;                                                                 \
      char *parents = (char *)malloc((int64_t)(deep * 15) * sizeof(char));                      \
      if (parents) {                                                                            \
        char *parents_ptr = parents;                                                            \
        for (int i = 0; i < deep; i++) {                                                        \
          if ((i > 0) && (i < deep)) {                                                          \
            *parents_ptr = ',';                                                                 \
            ++parents_ptr;                                                                      \
          }                                                                                     \
          sprintf(parents_ptr, "%p", *deep_ptr);                                                \
          parents_ptr += 14;                                                                    \
          deep_ptr++;                                                                           \
        }                                                                                       \
        flockfile(stderr);                                                                      \
        fprintf(stderr,                                                                         \
                "< 📘 > th_id=%zu time=%zu |> file=%-25s line=%-4d deep=%-2d function=%-30s " \
                "function_id=%-14p parents=<%s> |> position=init     |> " fmt "\n",             \
                pthread_self() % 1000, (timespec.tv_sec * 1000000) + (timespec.tv_nsec / 1000), \
                basename(__FILE__), __LINE__, deep, __func__, *(buffer + 2),                    \
                parents __VA_OPT__(, ) __VA_ARGS__);                                            \
        funlockfile(stderr);                                                                    \
        free(buffer);                                                                           \
        free(parents);                                                                          \
      } else {                                                                                  \
        free(buffer);                                                                           \
      }                                                                                         \
    }                                                                                           \
  }

#define LOG_END(fmt, ...)                                                                       \
  {                                                                                             \
    struct timespec timespec;                                                                   \
    clock_gettime(CLOCK_MONOTONIC, &timespec);                                                  \
    void **buffer = (void **)malloc(50 * sizeof(void *));                                       \
    if (buffer) {                                                                               \
      int deep = backtrace(buffer, 50);                                                         \
      void **deep_ptr = buffer;                                                                 \
      char *parents = (char *)malloc((int64_t)(deep * 15) * sizeof(char));                      \
      if (parents) {                                                                            \
        char *parents_ptr = parents;                                                            \
        for (int i = 0; i < deep; i++) {                                                        \
          if ((i > 0) && (i < deep)) {                                                          \
            *parents_ptr = ',';                                                                 \
            ++parents_ptr;                                                                      \
          }                                                                                     \
          sprintf(parents_ptr, "%p", *deep_ptr);                                                \
          parents_ptr += 14;                                                                    \
          deep_ptr++;                                                                           \
        }                                                                                       \
        flockfile(stderr);                                                                      \
        fprintf(stderr,                                                                         \
                "< 📘 > th_id=%zu time=%zu |> file=%-25s line=%-4d deep=%-2d function=%-30s " \
                "function_id=%-14p parents=<%s> |> position=end      |> " fmt "\n",             \
                pthread_self() % 1000, (timespec.tv_sec * 1000000) + (timespec.tv_nsec / 1000), \
                basename(__FILE__), __LINE__, deep, __func__, *(buffer + 2),                    \
                parents __VA_OPT__(, ) __VA_ARGS__);                                            \
        funlockfile(stderr);                                                                    \
        free(buffer);                                                                           \
        free(parents);                                                                          \
      } else {                                                                                  \
        free(buffer);                                                                           \
      }                                                                                         \
    }                                                                                           \
  }

#define LOG(fmt, ...)                                                                       \
  {                                                                                         \
    struct timespec timespec {};                                                            \
    clock_gettime(CLOCK_MONOTONIC, &timespec);                                              \
    void **buffer = (void **)malloc(50 * sizeof(void *));                                   \
    if (!buffer) return;                                                                    \
    int deep = backtrace(buffer, 50);                                                       \
    void **deep_ptr = buffer;                                                               \
    char *parents = (char *)malloc(static_cast<size_t>(deep * 15) * sizeof(char));          \
    if (!parents) {                                                                         \
      free(buffer);                                                                         \
      return;                                                                               \
    }                                                                                       \
    char *parents_ptr = parents;                                                            \
    for (int i = 0; i < deep; i++) {                                                        \
      if ((i > 0) && (i < deep)) {                                                          \
        *parents_ptr = ',';                                                                 \
        ++parents_ptr;                                                                      \
      }                                                                                     \
      sprintf(parents_ptr, "%p", *deep_ptr);                                                \
      parents_ptr += 14;                                                                    \
      deep_ptr++;                                                                           \
    }                                                                                       \
    flockfile(stderr);                                                                      \
    fprintf(stderr,                                                                         \
            "< 📘 > th_id=%zu time=%zu |> file=%-25s line=%-4d deep=%-2d function=%-30s " \
            "function_id=%-14p parents=<%s> |> position=punctual |> " fmt "\n",             \
            pthread_self() % 1000, (timespec.tv_sec * 1000000) + (timespec.tv_nsec / 1000), \
            basename(__FILE__), __LINE__, deep, __func__, *(buffer + 2),                    \
            parents __VA_OPT__(, ) __VA_ARGS__);                                            \
    funlockfile(stderr);                                                                    \
    free(buffer);                                                                           \
    free(parents);                                                                          \
  }

#endif  // DEBUG > 2

#endif  // DEBUG > 1

#endif  // DEBUG > 0

#endif  // DEBUG

#endif  // !LOGGER_H
