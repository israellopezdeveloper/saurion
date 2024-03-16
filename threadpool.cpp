// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "threadpool.hpp"

#include <pthread.h>

#include <cstdio>

pthread_mutex_t ThreadPool::global_mutex = PTHREAD_MUTEX_INITIALIZER;

ThreadPool::ThreadPool() : ThreadPool("ThreadPool", 4) {}

ThreadPool::ThreadPool(const char* name, size_t num_threads)
    : pool_name(name),
      num_threads(num_threads),
      threads(nullptr),
      should_stop(false),
      accept_tasks(true) {}

void ThreadPool::init() {
  threads = new pthread_t[num_threads];
  for (size_t i = 0; i < num_threads; ++i) {
    pthread_mutex_lock(&global_mutex);
    pthread_create(&threads[i], nullptr, thread_entry, this);
    char name[100];
    sprintf(name, "%s-%zu", pool_name, i);
    pthread_setname_np(threads[i], name);
    pthread_mutex_unlock(&global_mutex);
  }
}

void ThreadPool::stop() {
  accept_tasks = false;
  pthread_mutex_lock(&queue_mutex);
  while (!empty()) {
    pthread_cond_wait(&queue_condition, &queue_mutex);
  }
  pthread_mutex_unlock(&queue_mutex);
  should_stop = true;
  // Enviar broadcast a la variable condicional de la cola
  pthread_mutex_lock(&queue_mutex);
  pthread_cond_broadcast(&queue_condition);
  pthread_mutex_unlock(&queue_mutex);
  // Detener los hilos
  pthread_mutex_lock(&global_mutex);
  if (threads != nullptr) {
    for (size_t i = 0; i < num_threads; ++i) {
      pthread_join(threads[i], nullptr);
    }
  }
  pthread_mutex_unlock(&global_mutex);
  num_threads = 0;
}

void ThreadPool::add(uint32_t qid, void (*nfn)(void*), void* arg) {
  if (!accept_tasks) {
    return;
  }
  pthread_mutex_lock(&queue_mutex);
  queues[qid].push({nfn, arg});
  pthread_cond_signal(&queue_condition);
  pthread_mutex_unlock(&queue_mutex);
}

void ThreadPool::add(void (*nfn)(void*), void* arg) {
  add(0, nfn, arg);  // Agregar a la cola por defecto
}

void ThreadPool::new_queue(uint32_t /*unused*/, uint32_t /*unused*/) {
  // TODO revisar como hacer colas que no permitan ejecutar n tareas simultaneas
  // No es necesario inicializar mutex ni variable condicional para cada cola
}

void ThreadPool::remove_queue(uint32_t qid) {
  if (qid != 0) {  // No se puede eliminar la cola 0
    pthread_mutex_lock(&queue_mutex);
    queues.erase(qid);
    pthread_cond_signal(&queue_condition);
    pthread_mutex_unlock(&queue_mutex);
  }
}

bool ThreadPool::empty() {
  bool is_empty = true;
  for (const auto& pair : queues) {
    if (!pair.second.empty()) {
      is_empty = false;
      break;
    }
  }
  return is_empty;
}

void ThreadPool::thread_worker() {
  // Lógica del trabajador del hilo
  while (!should_stop) {
    // Buscar una tarea para ejecutar
    Task task{};
    bool found = false;
    for (auto& pair : queues) {
      pthread_mutex_lock(&queue_mutex);
      if (!pair.second.empty()) {
        task = pair.second.front();
        pair.second.pop();
        found = true;
      }
      pthread_mutex_unlock(&queue_mutex);
      if (found) {
        break;
      }
    }

    // Ejecutar la tarea si se encontró
    if (found) {
      task.function(task.argument);
    } else {
      // Esperar a que haya una tarea disponible
      // o a que se solicite detener el hilo
      pthread_mutex_lock(&queue_mutex);
      while (empty() && !should_stop) {
        pthread_cond_wait(&queue_condition, &queue_mutex);
      }
      pthread_mutex_unlock(&queue_mutex);
    }
  }
}

void* ThreadPool::thread_entry(void* arg) {
  auto* pool = static_cast<ThreadPool*>(arg);
  pool->thread_worker();
  return nullptr;
}

ThreadPool::~ThreadPool() {
  stop();
  delete[] threads;
  pthread_mutex_destroy(&queue_mutex);
  pthread_cond_destroy(&queue_condition);
}
