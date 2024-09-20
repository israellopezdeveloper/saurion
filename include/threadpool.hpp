#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

// Estructura para representar una tarea
#include <bits/types/sig_atomic_t.h>
#include <pthread.h>

#include <cstdint>
#include <queue>
#include <unordered_map>

enum StopFlags { KINDLY = 0x01, FORCE = 0x02 };

struct Task {
  void (*function)(void*);
  void* argument;

  explicit Task(void (*nfn)(void*), void* narg);
  Task(const Task&) = delete;
  Task(Task&&) = delete;
  Task& operator=(const Task&) = delete;
  Task& operator=(Task&&) = delete;
  ~Task() = default;
};

// Clase ThreadPool
class ThreadPool {
 private:
  typedef struct AsyncQueue {
   private:
    std::queue<Task*> m_queue;
    uint32_t m_max;
    uint32_t m_cnt;
    pthread_mutex_t m_mtx = PTHREAD_MUTEX_INITIALIZER;

   public:
    explicit AsyncQueue(uint32_t cnt);
    ~AsyncQueue();
    AsyncQueue(const AsyncQueue&) = delete;
    AsyncQueue& operator=(const AsyncQueue&) = delete;
    AsyncQueue(AsyncQueue&&) = delete;
    AsyncQueue& operator=(AsyncQueue&&) = delete;

    void push(Task* task);
    Task* front();
    void pop();

    bool empty();
  } AsyncQueue;
  typedef struct AsyncMultiQueue {
   private:
    std::unordered_map<uint32_t, AsyncQueue*> m_queues;
    std::unordered_map<uint32_t, AsyncQueue*>::iterator m_it;

   public:
    explicit AsyncMultiQueue();
    ~AsyncMultiQueue();
    AsyncMultiQueue(const AsyncMultiQueue&) = delete;
    AsyncMultiQueue& operator=(const AsyncMultiQueue&) = delete;
    AsyncMultiQueue(AsyncMultiQueue&&) = delete;
    AsyncMultiQueue& operator=(AsyncMultiQueue&&) = delete;

    void new_queue(uint32_t qid, uint32_t cnt);
    void remove_queue(uint32_t qid);

    void push(uint32_t qid, void (*nfn)(void*), void* arg);
    Task* front(uint32_t& qid);
    void pop(uint32_t qid);

    bool empty();
  } AsyncMultiQueue;

 public:
  ThreadPool();
  explicit ThreadPool(size_t num_threads);
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

 private:
  void wait_closeable();

 public:
  void wait_empty();
  ~ThreadPool();

 private:
  size_t m_nth;
  size_t m_started;
  AsyncMultiQueue m_queues;
  pthread_mutex_t m_q_mtx = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t m_q_cond = PTHREAD_COND_INITIALIZER;
  static pthread_mutex_t s_mtx;
  pthread_t* m_ths;
  volatile sig_atomic_t m_fstop;
  volatile sig_atomic_t m_faccept;

  void thread_worker();
  static void* thread_entry(void* arg);
};

#endif
