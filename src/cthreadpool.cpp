#include "cthreadpool.hpp"

ThreadPool *ThreadPool_create(size_t num_threads) { return new ThreadPool(num_threads); }

ThreadPool *ThreadPool_create_default(void) { return new ThreadPool(); }

void ThreadPool_init(ThreadPool *thp) { thp->init(); }

void ThreadPool_stop(ThreadPool *thp) { thp->stop(); }

void ThreadPool_add(ThreadPool *thp, uint32_t qid, void (*nfn)(void *), void *arg) { thp->add(qid, nfn, arg); }

void ThreadPool_add_default(ThreadPool *thp, void (*nfn)(void *), void *arg) { thp->add(nfn, arg); }

void ThreadPool_new_queue(ThreadPool *thp, uint32_t qid, uint32_t cnt) { thp->new_queue(qid, cnt); }

void ThreadPool_remove_queue(ThreadPool *thp, uint32_t qid) { thp->remove_queue(qid); }

bool ThreadPool_empty(ThreadPool *thp) { return thp->empty(); }

void ThreadPool_wait_empty(ThreadPool *thp) { thp->wait_empty(); }

void ThreadPool_destroy(ThreadPool *thp) { delete thp; }
