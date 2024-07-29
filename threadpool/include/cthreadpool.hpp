#ifndef THREADPOOL_H
#define THREADPOOL_H

#ifdef __cplusplus
#include "threadpool.hpp"
extern "C" {
#else
typedef struct ThreadPool ThreadPool;
#endif  // __cplusplus

ThreadPool *ThreadPool_create(size_t num_threads);

ThreadPool *ThreadPool_create_default(void);

void ThreadPool_init(ThreadPool *thp);

void ThreadPool_stop(ThreadPool *thp);

void ThreadPool_add(ThreadPool *thp, uint32_t qid, void (*nfn)(void *), void *arg);

void ThreadPool_add_default(ThreadPool *thp, void (*nfn)(void *), void *arg);

void ThreadPool_new_queue(ThreadPool *thp, uint32_t qid, uint32_t cnt);

void ThreadPool_remove_queue(ThreadPool *thp, uint32_t qid);

bool ThreadPool_empty(ThreadPool *thp);

void ThreadPool_wait_empty(ThreadPool *thp);

void ThreadPool_destroy(ThreadPool *thp);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // !THREADPOOL_H
