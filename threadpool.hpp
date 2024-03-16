#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

// Estructura para representar una tarea
#include <pthread.h>

#include <cstddef>
#include <cstdint>
#include <queue>
#include <unordered_map>

enum StopFlags { KINDLY = 0x01, FORCE = 0x02 };

struct Task {
  void (*function)(void*);
  void* argument;
};

// Clase ThreadPool
class ThreadPool {
 public:
  ThreadPool();
  ThreadPool(const char* name, size_t num_threads);
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  void init();
  void stop();
  void add(uint32_t qid, void (*nfn)(void*), void* arg);
  void add(void (*nfn)(void*), void* arg);
  void new_queue(uint32_t qid, uint32_t cnt);
  void remove_queue(uint32_t qid);
  bool empty();
  ~ThreadPool();

 private:
  const char* pool_name;
  size_t num_threads;
  std::unordered_map<uint32_t, std::queue<Task>> queues;
  pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t queue_condition = PTHREAD_COND_INITIALIZER;
  static pthread_mutex_t global_mutex;
  pthread_t* threads;
  bool should_stop;
  bool accept_tasks;

  void thread_worker();
  static void* thread_entry(void* arg);
};

#endif
