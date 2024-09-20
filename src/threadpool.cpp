// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "threadpool.hpp"

#include <pthread.h>

#include <stdexcept>

using TP = ThreadPool;

//********* Task ************
using T = Task;
T::Task(void (*nfn)(void*), void* narg) : function(nfn), argument(narg) {}

//********* AsyncQueue ************
TP::AsyncQueue::AsyncQueue(uint32_t cnt) : m_max(cnt), m_cnt(0) {}

TP::AsyncQueue::~AsyncQueue() { pthread_mutex_destroy(&m_mtx); }

void TP::AsyncQueue::push(Task* task) {
  pthread_mutex_lock(&m_mtx);
  m_queue.push(task);
  pthread_mutex_unlock(&m_mtx);
}
Task* TP::AsyncQueue::front() {
  pthread_mutex_lock(&m_mtx);
  if ((m_cnt >= m_max) && (m_max != 0)) {
    pthread_mutex_unlock(&m_mtx);
    throw std::out_of_range("reached max parallel tasks");
  }
  Task* task = m_queue.front();
  m_queue.pop();
  ++m_cnt;
  pthread_mutex_unlock(&m_mtx);
  return task;
}
void TP::AsyncQueue::pop() {
  pthread_mutex_lock(&m_mtx);
  --m_cnt;
  pthread_mutex_unlock(&m_mtx);
}

bool TP::AsyncQueue::empty() {
  pthread_mutex_lock(&m_mtx);
  bool empty = m_queue.empty();
  pthread_mutex_unlock(&m_mtx);
  return empty;
}

//********* AsyncMultiQueue ************

TP::AsyncMultiQueue::AsyncMultiQueue() {
  new_queue(0, 0);
  m_it = m_queues.begin();
}
TP::AsyncMultiQueue::~AsyncMultiQueue() {
  for (auto& queue : m_queues) {
    delete queue.second;
  }
}

void TP::AsyncMultiQueue::new_queue(uint32_t qid, uint32_t cnt) {
  if (m_queues.find(qid) != m_queues.end()) {
    throw std::out_of_range("queue already exists");
  }
  m_queues.emplace(qid, new TP::AsyncQueue(cnt));
}
void TP::AsyncMultiQueue::remove_queue(uint32_t qid) {
  auto queue = m_queues.find(qid);
  if (queue == m_queues.end()) {
    throw std::out_of_range("queue not found");
  }
  delete queue->second;
  m_queues.erase(qid);
}

void TP::AsyncMultiQueue::push(uint32_t qid, void (*nfn)(void*), void* arg) {
  m_queues.at(qid)->push(new Task{nfn, arg});
}
Task* TP::AsyncMultiQueue::front(uint32_t& qid) {
  if (empty()) {
    throw std::out_of_range("empty queue");
  }
  auto newit = m_it;
  ++newit;
  while (newit != m_it) {
    if (newit == m_queues.end()) {
      newit = m_queues.begin();
    }
    if (!newit->second->empty()) {
      break;
    }
    ++newit;
  }
  Task* task = newit->second->front();
  qid = newit->first;
  m_it = newit;
  return task;
}
void TP::AsyncMultiQueue::pop(uint32_t qid) { m_queues.at(qid)->pop(); }

bool TP::AsyncMultiQueue::empty() {
  for (auto& queue : m_queues) {
    if (!queue.second->empty()) {
      return false;
    }
  }
  return true;
}

//********* ThreadPool ************

pthread_mutex_t TP::s_mtx = PTHREAD_MUTEX_INITIALIZER;

TP::ThreadPool() : ThreadPool(4) {}

TP::ThreadPool(size_t num_threads)
    : m_nth(num_threads < 2 ? 2 : num_threads),
      m_started(0),
      m_ths(new pthread_t[m_nth]{0}),
      m_fstop(0),
      m_faccept(0) {}

void TP::init() {
  if (m_started != 0) {
    return;
  }
  m_faccept = 1;
  m_fstop = 0;
  pthread_mutex_lock(&s_mtx);
  for (size_t i = 0; i < m_nth; ++i) {
    pthread_create(&m_ths[i], nullptr, thread_entry, this);
    ++m_started;
  }
  pthread_mutex_unlock(&s_mtx);
}
void TP::stop() {
  if (m_started == 0) {
    return;
  }
  m_fstop = 0;
  m_faccept = 0;
  wait_empty();

  pthread_mutex_lock(&m_q_mtx);
  m_fstop = 1;
  pthread_cond_broadcast(&m_q_cond);
  pthread_mutex_unlock(&m_q_mtx);
  // Detener los hilos
  for (size_t i = 0; i < m_started; ++i) {
    pthread_join(m_ths[i], nullptr);
  }
  m_started = 0;
  m_nth = 0;
}
void TP::add(uint32_t qid, void (*nfn)(void*), void* arg) {
  if (m_faccept == 0) {
    throw std::logic_error("threadpool already closed");
  }
  if (nfn == nullptr) {
    throw std::logic_error("function pointer cannot be null");
  }
  bool failed = false;
  pthread_mutex_lock(&m_q_mtx);
  try {
    m_queues.push(qid, nfn, arg);
  } catch (const std::out_of_range& e) {
    failed = true;
  }
  pthread_cond_signal(&m_q_cond);
  pthread_mutex_unlock(&m_q_mtx);
  if (failed) {
    throw std::out_of_range("queue not found");
  }
}
void TP::add(void (*nfn)(void*), void* arg) {
  add(0, nfn, arg);  // Agregar a la cola por defecto
}
void TP::new_queue(uint32_t qid, uint32_t cnt) {
  pthread_mutex_lock(&m_q_mtx);
  try {
    m_queues.new_queue(qid, cnt);
  } catch (const std::out_of_range& e) {
    pthread_mutex_unlock(&m_q_mtx);
    throw e;
  }
  pthread_mutex_unlock(&m_q_mtx);
}
void TP::remove_queue(uint32_t qid) {
  if (qid != 0) {
    bool failed = false;
    pthread_mutex_lock(&m_q_mtx);
    try {
      m_queues.remove_queue(qid);
    } catch (const std::out_of_range& e) {
      failed = true;
    }
    pthread_cond_signal(&m_q_cond);
    pthread_mutex_unlock(&m_q_mtx);
    if (failed) {
      throw std::out_of_range("queue not found");
    }
  }
}
bool TP::empty() { return m_queues.empty(); }
void TP::wait_closeable() {
  pthread_mutex_lock(&m_q_mtx);
  while (empty() && m_fstop == 0) {
    pthread_cond_wait(&m_q_cond, &m_q_mtx);
  }
  pthread_mutex_unlock(&m_q_mtx);
}
void TP::wait_empty() {
  pthread_mutex_lock(&m_q_mtx);
  while (!m_queues.empty()) {
    pthread_cond_wait(&m_q_cond, &m_q_mtx);
  }
  pthread_mutex_unlock(&m_q_mtx);
}
TP::~ThreadPool() {
  stop();
  delete[] m_ths;
  pthread_mutex_destroy(&s_mtx);
}

void TP::thread_worker() {
  // Lógica del trabajador del hilo
  uint32_t qid = 0;
  while (m_fstop == 0) {
    // Buscar una tarea para ejecutar
    try {
      pthread_mutex_lock(&m_q_mtx);
      Task* task = m_queues.front(qid);
      pthread_cond_signal(&m_q_cond);
      pthread_mutex_unlock(&m_q_mtx);
      try {
        task->function(task->argument);
      } catch (...) {
      }
      delete task;  // TODO delete task
      pthread_mutex_lock(&m_q_mtx);
      m_queues.pop(qid);
      pthread_cond_broadcast(&m_q_cond);
      pthread_mutex_unlock(&m_q_mtx);
    } catch (const std::out_of_range& e) {
      pthread_mutex_unlock(&m_q_mtx);
      wait_closeable();
    }
  }
}
void* TP::thread_entry(void* arg) {
  auto* pool = static_cast<TP*>(arg);
  pool->thread_worker();
  return nullptr;
}
