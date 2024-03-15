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

#include "asyncclientsocket.hpp"
#include "asyncserversocket.hpp"
#include "connection.hpp"

typedef struct ConnectionParam {
  Connection* conn;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  volatile sig_atomic_t counter = 0;

  ~ConnectionParam() { delete conn; }
} ConnectionParam;

typedef struct ReadParam {
  bool (*verification_function)(char*);
  volatile sig_atomic_t counterServer = 0;
  volatile sig_atomic_t counterClient = 0;
} ReadParam;

ConnectionParam* conn_p = nullptr;
ReadParam* read_p = nullptr;

// ====================================================================================
// Initialization functions
// ====================================================================================

void init_params(ConnectionParam*& cp, ReadParam*& rp) {
  cp = new ConnectionParam{nullptr, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};
  rp = new ReadParam{[](char* msg) -> bool {
    regex_t reegex;
    int rc = regcomp(&reegex, "^a*$", 0);
    auto res = regexec(&reegex, msg, 0, nullptr, 0) == 0;
    regfree(&reegex);
    return res;
  }};
}

void create_server(AsyncServerSocket& server, ConnectionParam*& cp, ReadParam*& rp) {
  server.on_connected(
      [](Connection& conn, void* arg) -> void {
        auto* p = static_cast<ConnectionParam*>(arg);
        pthread_mutex_lock(&p->mutex);
        p->conn = &conn;
        pthread_cond_broadcast(&p->cond);
        pthread_mutex_unlock(&p->mutex);
        ++p->counter;
      },
      cp);
  server.on_readed(
      [](int fd, char* buffer, ssize_t bytes_read, void* arg) -> void {
        auto* rp = static_cast<ReadParam*>(arg);
        if (rp->verification_function(buffer)) {
          rp->counterServer += bytes_read;
        }
      },
      rp);
  server.start(2000);
}

void create_client(AsyncClientSocket& client, ConnectionParam*& cp, ReadParam*& rp) {
  client.on_connected(
      [](Connection&, void* arg) -> void {
        auto* p = static_cast<ConnectionParam*>(arg);
        ++p->counter;
      },
      cp);
  client.on_readed(
      [](int fd, char* buffer, ssize_t bytes_read, void* arg) -> void {
        auto* rp = static_cast<ReadParam*>(arg);
        if (rp->verification_function(buffer)) {
          rp->counterClient += bytes_read;
        }
      },
      rp);
  client.connect("127.0.0.1", 2000);
}

void init(AsyncServerSocket& server, AsyncClientSocket& client, ConnectionParam*& cp,
          ReadParam*& rp) {
  init_params(cp, rp);
  create_server(server, cp, rp);
  create_client(client, cp, rp);
}

void stop(AsyncServerSocket& server, AsyncClientSocket& client, ConnectionParam*& cp,
          ReadParam*& rp) {
  server.stop();
  client.stop();
  delete cp;
  delete rp;
}

// ====================================================================================
// Step functions
// ====================================================================================

void wait_till_connect(ConnectionParam* cp) {
  pthread_mutex_lock(&cp->mutex);
  while (cp->conn == nullptr) {
    pthread_cond_wait(&cp->cond, &cp->mutex);
  }
  pthread_mutex_unlock(&cp->mutex);
}

void send_to(AsyncServerSocket& server, ConnectionParam* cp, int msgs, size_t msgs_size) {
  try {
    if (cp->conn != nullptr) {
      for (int i = 0; i < msgs; ++i) {
        auto* msg = new char[msgs_size];
        memset(msg, 'a', msgs_size);
        server.send(cp->conn, msg, msgs_size);
        delete[] msg;
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    exit(EXIT_FAILURE);
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
    exit(EXIT_FAILURE);
  }
}

void wait_till_read(ReadParam* rp, size_t s_expected, size_t c_expected) {
  while (rp->counterServer < s_expected || rp->counterClient < c_expected) {
    usleep(60);
  }
}

// ====================================================================================
// Main
// ====================================================================================

void basic_tests(int times, size_t len) {
  {
    char title[100];
    sprintf(title, "TIMES: %d, LENGTH: %zu", times, len);
    char underline[100];
    sprintf(underline, "%s", std::string(strlen(title), '=').c_str());
    printf("%s\n", underline);
    printf("%s\n", title);
    printf("%s\n", underline);
  }
  printf("Server sending %d messages of %zu bytes ... ", times, len);
  {
    AsyncServerSocket server;
    AsyncClientSocket client;
    init(server, client, conn_p, read_p);

    wait_till_connect(conn_p);

    send_to(server, conn_p, times, len);

    wait_till_read(read_p, 0, times * (len + 1));

    stop(server, client, conn_p, read_p);
  }
  printf("OK\n");

  printf("Client sending %d messages of %zu bytes ... ", times, len);
  {
    AsyncServerSocket server;
    AsyncClientSocket client;
    init(server, client, conn_p, read_p);

    wait_till_connect(conn_p);

    send_to(client, times, len);

    wait_till_read(read_p, times * (len + 1), 0);

    stop(server, client, conn_p, read_p);
  }
  printf("OK\n");

  printf("Server and Client sending %d messages of %zu bytes ... ", times, len);
  {
    AsyncServerSocket server;
    AsyncClientSocket client;
    init(server, client, conn_p, read_p);

    wait_till_connect(conn_p);

    send_to(server, conn_p, times, len);
    send_to(client, times, len);

    wait_till_read(read_p, times * (len + 1), times * (len + 1));

    stop(server, client, conn_p, read_p);
  }
  printf("OK\n");
}

int main() {
  int times = 100;
  size_t len = 15;
  basic_tests(times, len);

  printf("\n");

  len = 1000;
  basic_tests(times, len);

  printf("\n");

  times = 1000;
  len = 15;
  basic_tests(times, len);

  printf("\n");

  times = 1000;
  len = 100;
  basic_tests(times, len);

  return EXIT_SUCCESS;
}
