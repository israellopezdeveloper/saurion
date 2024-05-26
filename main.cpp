// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include <pthread.h>
#include <regex.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "sockets/asyncclientsocket.hpp"
#include "sockets/asyncserversocket.hpp"

constexpr const char* RESET = "\033[0m";
constexpr const char* RED = "\033[31m";
constexpr const char* GREEN = "\033[32m";

typedef struct ConnectionParam {
  ConnectionParam(const ConnectionParam&) = delete;
  ConnectionParam(ConnectionParam&&) = delete;
  ConnectionParam& operator=(const ConnectionParam&) = delete;
  ConnectionParam& operator=(ConnectionParam&&) = delete;

  Connection* conn = nullptr;
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
  volatile sig_atomic_t counter = 0;

  ~ConnectionParam() = default;
} ConnectionParam;

typedef struct ReadParam {
  explicit ReadParam(bool (*verification_function)(char*))
      : verification_function(verification_function) {}
  ReadParam(const ReadParam&) = delete;
  ReadParam(ReadParam&&) = delete;
  ReadParam& operator=(const ReadParam&) = delete;
  ReadParam& operator=(ReadParam&&) = delete;

  bool (*verification_function)(char*);
  volatile sig_atomic_t counterServer = 0;
  volatile sig_atomic_t counterClient = 0;

  ~ReadParam() = default;
} ReadParam;

ConnectionParam* conn_p = nullptr;
ReadParam* read_p = nullptr;

// ====================================================================================
// Initialization functions
// ====================================================================================

void init_params(ConnectionParam*& ncp, ReadParam*& nrp) {
  ncp = new ConnectionParam{nullptr, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};
  nrp = new ReadParam{[](char* msg) -> bool {
    regex_t reegex;
    regcomp(&reegex, "^a*$", 0);
    auto res = regexec(&reegex, msg, 0, nullptr, 0) == 0;
    regfree(&reegex);
    return res;
  }};
}

void create_server(AsyncServerSocket& server, ConnectionParam*& ncp, ReadParam*& nrp) {
  server.on_connected(
      [](Connection& conn, void* arg) -> void {
        auto* ptr = static_cast<ConnectionParam*>(arg);
        pthread_mutex_lock(&ptr->mutex);
        ptr->conn = &conn;
        pthread_cond_broadcast(&ptr->cond);
        pthread_mutex_unlock(&ptr->mutex);
        ++ptr->counter;
      },
      ncp);
  server.on_readed(
      [](int /*nfd*/, char* buffer, ssize_t bytes_read, void* arg) -> void {
        auto* ptr = static_cast<ReadParam*>(arg);
        if (ptr->verification_function(buffer)) {
          ptr->counterServer += static_cast<int>(bytes_read);
        }
      },
      nrp);
  server.start(2000);
}

void create_client(AsyncClientSocket& clt, ConnectionParam*& ncp, ReadParam*& nrp) {
  clt.on_connected(
      [](Connection&, void* arg) -> void {
        auto* ptr = static_cast<ConnectionParam*>(arg);
        ++ptr->counter;
      },
      ncp);
  clt.on_readed(
      [](int /*nfd*/, char* buffer, ssize_t bytes_read, void* arg) -> void {
        auto* ptr = static_cast<ReadParam*>(arg);
        if (ptr->verification_function(buffer)) {
          ptr->counterClient += static_cast<int>(bytes_read);
        }
      },
      nrp);
  clt.connect("127.0.0.1", 2000);
}

void init(AsyncServerSocket& server, AsyncClientSocket& client, ConnectionParam*& ncp,
          ReadParam*& nrp) {
  init_params(ncp, nrp);
  create_server(server, ncp, nrp);
  create_client(client, ncp, nrp);
}

void stop(AsyncServerSocket& svr, AsyncClientSocket& clt, ConnectionParam*& ncp, ReadParam*& nrp) {
  clt.stop();
  svr.stop();
  // TODO asegurar que cuando se para es por que no hay ningún hilo ejecutandose
  delete ncp;
  delete nrp;
}

// ====================================================================================
// Step functions
// ====================================================================================

void wait_till_connect(ConnectionParam* ncp) {
  pthread_mutex_lock(&ncp->mutex);
  while (ncp->conn == nullptr) {
    pthread_cond_wait(&ncp->cond, &ncp->mutex);
  }
  pthread_mutex_unlock(&ncp->mutex);

  usleep(5000);
}

void send_to(AsyncServerSocket& server, ConnectionParam* ncp, int msgs, size_t msgs_size) {
  try {
    if (ncp->conn != nullptr) {
      for (int i = 0; i < msgs; ++i) {
        auto* msg = new char[msgs_size];
        memset(msg, 'a', msgs_size);
        server.send(ncp->conn, msg, msgs_size);
        delete[] msg;
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
}

void send_to(AsyncClientSocket& client, int msgs, size_t msgs_size) {
  try {
    for (int i = 0; i < msgs; ++i) {
      auto* msg = new char[msgs_size];
      memset(msg, 'a', msgs_size);
      client.send(msg, msgs_size);
      delete[] msg;
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
}

void wait_till_read(ReadParam* nrp, size_t s_extd, size_t c_extd) {
  int s_extd_int = static_cast<int>(s_extd);
  int c_extd_int = static_cast<int>(c_extd);
  fprintf(stderr, "\n");
  while (nrp->counterServer < s_extd_int || nrp->counterClient < c_extd_int) {
    usleep(6000);
    fprintf(stderr, "\rServer: %d, Client: %d", nrp->counterServer, nrp->counterClient);
    fflush(stderr);
  }
}

// ====================================================================================
// Main
// ====================================================================================

void basic_tests(int times, size_t len) {
  {
    char title[100];
    sprintf(title, " - TIMES: %d, LENGTH: %zu", times, len);
    printf("%s\n", title);
  }

  {
    printf("   - Server sending %d messages of %zu bytes . . . ", times, len);
    fflush(stdout);
  }
  {
    AsyncServerSocket server;
    AsyncClientSocket client;
    init(server, client, conn_p, read_p);

    wait_till_connect(conn_p);

    send_to(server, conn_p, times, len);

    wait_till_read(read_p, 0, times * (len - 1));

    stop(server, client, conn_p, read_p);
  }
  {
    printf("%sOK%s\n", GREEN, RESET);
    fflush(stdout);
  }

  {
    printf("   - Client sending %d messages of %zu bytes ... ", times, len);
    fflush(stdout);
  }
  {
    AsyncServerSocket server;
    AsyncClientSocket client;
    init(server, client, conn_p, read_p);

    wait_till_connect(conn_p);

    send_to(client, times, len);

    wait_till_read(read_p, times * (len - 1), 0);

    stop(server, client, conn_p, read_p);
  }
  {
    printf("%sOK%s\n", GREEN, RESET);
    fflush(stdout);
  }

  {
    printf("   - Server and Client sending %d messages of %zu bytes ... ", times, len);
    fflush(stdout);
  }
  {
    AsyncServerSocket server;
    AsyncClientSocket client;
    init(server, client, conn_p, read_p);

    wait_till_connect(conn_p);

    send_to(server, conn_p, times, len);
    send_to(client, times, len);

    wait_till_read(read_p, times * (len - 1), times * (len - 1));

    stop(server, client, conn_p, read_p);
  }
  {
    printf("%sOK%s\n", GREEN, RESET);
    fflush(stdout);
  }
}

//*************************************************************************************
// Threadpool
//*************************************************************************************

typedef struct TaskParam {
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
  uint16_t wait;
  uint64_t done = 0;
  TaskParam(const TaskParam&) = delete;
  TaskParam(TaskParam&&) = delete;
  TaskParam& operator=(const TaskParam&) = delete;
  TaskParam& operator=(TaskParam&&) = delete;
  explicit TaskParam(uint16_t nwait) : wait(nwait) {}
  ~TaskParam() {
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&condition);
  }
} TaskParam;
typedef std::vector<TaskParam*> WaitingList;
typedef struct Queue {
  Queue(const Queue&) = delete;
  Queue(Queue&&) = delete;
  Queue& operator=(const Queue&) = delete;
  Queue& operator=(Queue&&) = delete;
  Queue(uint8_t qid, uint8_t n_threads, WaitingList* list, long expected)
      : qid(qid), n_threads(n_threads), list(list), expected(expected) {}
  uint8_t qid;
  uint8_t n_threads;
  WaitingList* list;
  long expected;

  ~Queue() {
    for (auto* ptr : *list) {
      delete ptr;
    }
    delete list;
  }
} Queue;
typedef std::vector<Queue*> Queues;

bool check(const char* title, Queues& queues) {
  {
    printf("%s . . . ", title);
    fflush(stdout);
  }
  char* result = new char[1]{0};
  {
    ThreadPool pool(5);
    for (auto& queue : queues) {
      pool.new_queue(queue->qid, queue->n_threads);
      for (auto& item : *queue->list) {
        pool.add(
            queue->qid,
            [](void* arg) {
              auto* ptr = static_cast<TaskParam*>(arg);
              struct timespec res {};
              clock_gettime(CLOCK_REALTIME, &res);
              pthread_mutex_lock(&ptr->mutex);
              ptr->done = res.tv_nsec;
              pthread_cond_signal(&ptr->condition);
              pthread_mutex_unlock(&ptr->mutex);
            },
            item);
      }
    }
    struct timespec res {};
    clock_gettime(CLOCK_REALTIME, &res);
    long last = res.tv_nsec;
    pool.init();
    for (auto& queue : queues) {
      double waitingtime = 0;
      for (auto& item : *queue->list) {
        pthread_mutex_lock(&item->mutex);
        while (item->done == 0) {
          pthread_cond_wait(&item->condition, &item->mutex);
        }
        pthread_mutex_unlock(&item->mutex);
        double diff = static_cast<double>(item->done - last) / 1000;
        waitingtime = (waitingtime < diff) ? diff : waitingtime;
      }
      pool.remove_queue(queue->qid);
      if (waitingtime > (static_cast<double>(queue->expected) * 1.3)) {
        char* result2 = new char[100 + strlen(result)];
        sprintf(result2, "%s[Queue %d] Waiting time: %f\n", result, queue->qid, waitingtime);
        delete[] result;
        result = result2;
      }
    }
  }
  auto bool_res = strcmp(result, "") == 0;
  {
    if (bool_res) {
      printf("%s[OK]%s\n", GREEN, RESET);
    } else {
      printf("%s[KO]%s\n%s", RED, RESET, result);
    }
    delete[] result;
  }
  return bool_res;
}

void check_multiple_threadpools(int n, bool init, bool tasks) {
  {
    printf(" - Is available %d threadpools %sinitialized %s? . . . ", n, init ? "" : "un",
           tasks ? "with tasks" : "without tasks");
    fflush(stdout);
  }
  {
    auto** pools = new ThreadPool*[n];
    for (int i = 0; i < n; i++) {
      pools[i] = new ThreadPool(5);
    }
    if (tasks) {
      for (int i = 0; i < n; i++) {
        for (int j = 0; j < 10; j++) {
          pools[i]->add(
              0,
              [](void* arg) {
                auto* ptr = static_cast<TaskParam*>(arg);
                usleep(ptr->wait);
                struct timespec res {};
                clock_gettime(CLOCK_REALTIME, &res);
                pthread_mutex_lock(&ptr->mutex);
                ptr->done = res.tv_nsec;
                pthread_cond_signal(&ptr->condition);
                pthread_mutex_unlock(&ptr->mutex);
                pthread_mutex_destroy(&ptr->mutex);
                pthread_cond_destroy(&ptr->condition);
                delete ptr;
              },
              new TaskParam(500));
        }
      }
    }
    if (init) {
      for (int i = 0; i < n; i++) {
        pools[i]->init();
      }
    }
    for (int i = 0; i < n; i++) {
      delete pools[i];
    }
    delete[] pools;
  }
  { printf("%s[OK]%s\n", GREEN, RESET); }
}

void threadpool_tests() {
  {
    printf("THREADPOOL TESTS\n");
    printf("================\n");
    fflush(stdout);
  }
  {
    printf(" - Checking not empty queue . . . ");
    fflush(stdout);
  }
  {
    ThreadPool pool(24);
    pool.init();
    for (int i = 0; i < 10000; ++i) {
      pool.add(
          [](void* arg) {
            auto* ptr = static_cast<TaskParam*>(arg);
            usleep(ptr->wait);
            delete ptr;
          },
          new TaskParam(5));
    }
  }
  { printf("%s[OK]%s\n", GREEN, RESET); }

  Queues queues{new Queue{
                    0,              // Queue 0
                    0,              // 0 threads
                    new WaitingList{// Waiting list
                                    new TaskParam{500}, new TaskParam{1000}, new TaskParam{600}},
                    1000  // Expected waiting time
                },
                new Queue{
                    1,              // Queue 1
                    1,              // 1 threads
                    new WaitingList{// Waiting list
                                    new TaskParam{100}, new TaskParam{10}, new TaskParam{500}},
                    610  // Expected waiting time

                }};
  check(" - Checking 2 queues", queues);
  for (auto& item : queues) {
    delete item;
  }

  check_multiple_threadpools(5, false, false);
  check_multiple_threadpools(5, true, false);
  check_multiple_threadpools(5, false, true);
  check_multiple_threadpools(5, true, true);

  {
    printf(" - Doble stop . . . ");
    fflush(stdout);
  }
  {
    ThreadPool pool(5);
    ThreadPool pool2(5);
    pool.stop();
    pool.stop();
    pool2.stop();
    pool2.stop();
  }
  { printf("%s[OK]%s\n", GREEN, RESET); }
}

void asyncsocket_tests() {
  {
    printf("\n");
    printf("ASYNCSOCKET TESTS\n");
    printf("=================\n");
    fflush(stdout);
  }
  int times = 10;
  size_t len = 10;
  // basic_tests(times, len);

  // printf("\n");

  // len = 1000;
  // basic_tests(times, len);

  // printf("\n");

  // times = 1000;
  // len = 15;
  // basic_tests(times, len);

  // printf("\n");

  // times = 1000;
  // len = 100;
  // basic_tests(times, len);

  // printf("\n");

  times = 200;
  len = 100;
  basic_tests(times, len);

  // {
  //   printf("Checking empty queue . . . ");
  //   fflush(stdout);
  // }
  // {
  //   ThreadPool pool(5);
  //   pool.init();
  // }
  // {
  //   printf("%s[OK]%s\n", GREEN, RESET);
  //   fflush(stdout);
  // }
}

// **************************************************************

void standalone_tests() {
  auto times = 10000;
  auto length = 10000;
  {
    printf("STANDALONE TESTS\n");
    printf("================\n");
    fflush(stdout);
  }
  {
    AsyncServerSocket server;
    AsyncClientSocket client;
    init_params(conn_p, read_p);
    create_server(server, conn_p, read_p);
    wait_till_connect(conn_p);
    send_to(server, conn_p, times, length);
    usleep(1000000);
  }
}

int main() {
  // threadpool_tests();

  asyncsocket_tests();
  // TODO mejorar la capacidad de incrementar el tamanio de los datos y el numero de iteraciones

  return EXIT_SUCCESS;
}
