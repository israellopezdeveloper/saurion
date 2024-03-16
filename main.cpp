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
  svr.stop();
  clt.stop();
  // TODO asegurar que cuando se para es por que no hay ningún hilo ejecutandose
  usleep(100);
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
  while (nrp->counterServer < s_extd_int || nrp->counterClient < c_extd_int) {
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

  printf("\n");

  times = 20000;
  len = 100;
  basic_tests(times, len);

  // TODO mejorar la capacidad de incrementar el tamanio de los datos y el numero de iteraciones

  return EXIT_SUCCESS;
}
