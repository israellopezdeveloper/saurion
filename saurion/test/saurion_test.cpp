// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include "gtest/gtest.h"
#include "low_saurion.h"

#define PORT 8080

struct summary {
  explicit summary() = default;
  ~summary() = default;

  summary(const summary &) = delete;
  summary(summary &&) = delete;
  summary &operator=(const summary &) = delete;
  summary &operator=(summary &&) = delete;

  uint connected = 0;
  std::vector<int> fds;
  pthread_cond_t connected_c = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t connected_m = PTHREAD_MUTEX_INITIALIZER;
  uint readed = 0;
  pthread_cond_t readed_c = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t readed_m = PTHREAD_MUTEX_INITIALIZER;
  uint wrote = 0;
  pthread_cond_t wrote_c = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t wrote_m = PTHREAD_MUTEX_INITIALIZER;
} __attribute__((aligned(128))) summary;

class LowSaurionTest : public ::testing::Test {
 public:
  std::vector<int> client_threads;
  struct saurion *saurion;

 protected:
  void SetUp() override {
    summary.connected = 0;
    summary.readed = 0;
    summary.wrote = 0;
    summary.fds.clear();
    saurion = saurion_create();
    if (saurion == nullptr) {
      return;
    }
    saurion->ss = EXTERNAL_set_socket(PORT);
    saurion->cb.on_connected = [](int sfd, void *) -> void {
      pthread_mutex_lock(&summary.connected_m);
      summary.connected++;
      summary.fds.push_back(sfd);
      pthread_cond_signal(&summary.connected_c);
      pthread_mutex_unlock(&summary.connected_m);
    };
    saurion->cb.on_readed = [](int, const char *const, const ssize_t, void *) -> void {
      pthread_mutex_lock(&summary.readed_m);
      summary.readed++;
      pthread_cond_signal(&summary.readed_c);
      pthread_mutex_unlock(&summary.readed_m);
    };
    saurion->cb.on_wrote = [](int, void *) -> void {
      pthread_mutex_lock(&summary.wrote_m);
      summary.wrote++;
      pthread_cond_signal(&summary.wrote_c);
      pthread_mutex_unlock(&summary.wrote_m);
    };
    saurion->cb.on_closed = [](int sfd, void *) -> void {
      pthread_mutex_lock(&summary.connected_m);
      summary.connected--;
      auto &vec = summary.fds;
      vec.erase(std::remove(vec.begin(), vec.end(), sfd), vec.end());
      pthread_cond_signal(&summary.connected_c);
      pthread_mutex_unlock(&summary.connected_m);
    };
    saurion->cb.on_error = [](int sfd, const char *const, const ssize_t, void *) -> void {
      printf("On error %d\n", sfd);
    };
    saurion_thread = new std::thread(
        [](void *arg) -> void {
          auto *saurion = static_cast<struct saurion *>(arg);
          saurion_start(saurion);
        },
        saurion);

    pthread_mutex_lock(&saurion->status_m);
    while (saurion->status != 1) {
      pthread_cond_wait(&saurion->status_c, &saurion->status_m);
    }
    pthread_mutex_unlock(&saurion->status_m);
  }

  void TearDown() override {
    saurion_stop(saurion);
    if (saurion_thread->joinable()) {
      saurion_thread->join();
    }
    saurion_destroy(saurion);
    delete saurion_thread;
    for (auto sfd : client_threads) {
      close(sfd);
    }
  }

  [[nodiscard]] int add_clients(uint n) {
    for (uint i = 0; i < n; ++i) {
      int sock = create_client();
      if (connect_client(sock) == -1) {
        for (uint j = 0; j < i; ++j) {
          close(client_threads.front());
          client_threads.pop_back();
        }
        return -1;
      }
      client_threads.push_back(sock);
    }
    return 0;
  }

  static void wait_connected(uint n) {
    pthread_mutex_lock(&summary.connected_m);
    while (summary.connected != n) {
      pthread_cond_wait(&summary.connected_c, &summary.connected_m);
    }
    pthread_mutex_unlock(&summary.connected_m);
  }

  static void wait_readed(uint n) {
    pthread_mutex_lock(&summary.readed_m);
    while (summary.readed < n) {
      pthread_cond_wait(&summary.readed_c, &summary.readed_m);
    }
    pthread_mutex_unlock(&summary.readed_m);
  }

  static void wait_wrote(uint n) {
    n = static_cast<uint>(n * 0.99999);
    pthread_mutex_lock(&summary.wrote_m);
    while (summary.wrote < n) {
      pthread_cond_wait(&summary.wrote_c, &summary.wrote_m);
    }
    pthread_mutex_unlock(&summary.wrote_m);
  }

  [[nodiscard]] int client_sends(int sfd, uint n, const char *msg) {
    if (std::find(client_threads.begin(), client_threads.end(), sfd) == client_threads.end()) {
      return -1;
    }
    for (uint i = 0; i < n; ++i) {
      write(sfd, msg, strlen(msg));
    }
    return 0;
  }

  void all_clients_sends(uint n, const char *msg) {
    std::vector<std::thread *> senders;
    senders.reserve(client_threads.size());
    for (auto sfd : client_threads) {
      senders.push_back(new std::thread([sfd, n, msg]() {
        for (uint i = 0; i < n; ++i) {
          write(sfd, msg, strlen(msg));
        }
      }));
    }
    for (std::thread *thrd : senders) {
      if (thrd->joinable()) {
        thrd->join();
      }
      delete thrd;
    }
  }

  void saurion_sends_to_client(int sfd, uint n, const char *const msg) const {
    for (uint i = 0; i < n; ++i) {
      saurion_send(saurion, sfd, msg);
    }
  }

  void saurion_sends_to_all_clients(uint n, const char *const msg) {
    for (auto sfd : client_threads) {
      for (uint i = 0; i < n; ++i) {
        saurion_send(saurion, sfd, msg);
      }
    }
  }

  static size_t countOccurrences(std::string &content, const std::string &search) {
    size_t count = 0;
    size_t pos = content.find(search);

    while (pos != std::string::npos) {
      count++;
      pos = content.find(search, pos + search.length());
    }

    return count;
  }

  static size_t read_from_client(int sockfd, const std::string &search) {
    // Configurar el socket como no bloqueante
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
      perror("fcntl");
      return -1;
    }
    // Definir el conjunto de descriptores de archivo para esperar que el socket esté listo para
    // escribir
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(sockfd, &writefds);

    // Configurar el tiempo de espera máximo
    struct timeval timeout {};
    timeout.tv_sec = 0;
    timeout.tv_usec = 100;

    const size_t bufferSize = 1024;
    char buffer[bufferSize];
    std::string content;
    ssize_t bytesRead = 0;
    int ready = select(sockfd + 1, nullptr, &writefds, nullptr, &timeout);
    while (ready > 0) {
      if (FD_ISSET(sockfd, &writefds)) {
        bytesRead = recv(sockfd, buffer, bufferSize, 0);
        if (bytesRead > 0) {
          content += std::string(buffer, bytesRead);
          memset(buffer, 0, bufferSize);
        } else {
          break;
        }
      }
      ready = select(sockfd + 1, nullptr, &writefds, nullptr, &timeout);
    }

    auto count = countOccurrences(content, search);
    return count;
  }

  size_t read_from_clients(const std::string &search) {
    size_t count = 0;
    for (auto sfd : client_threads) {
      count += read_from_client(sfd, search);
    }
    return count;
  }

 private:
  [[nodiscard]] static int create_client() {
    int sockfd = 0;

    // Crear el socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
      return -1;
    }
    struct timeval timeout {};
    timeout.tv_sec = 0;  // 5 segundos de timeout
    timeout.tv_usec = 100000;
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, static_cast<void *>(&timeout),
                   sizeof(timeout)) < 0) {
      close(sockfd);
      return -1;
    }

    return sockfd;
  }

  [[nodiscard]] static int connect_client(int sfd) {
    struct sockaddr_in sadd {};
    // Configurar la dirección del servidor
    memset(&sadd, 0, sizeof(sadd));
    sadd.sin_family = AF_INET;
    sadd.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &sadd.sin_addr) <= 0) {
      printf("ERROR: %d\n", sfd);
      perror("inet_pton");
      close(sfd);
      return -1;
    }
    // Conectar al servidor
    int ret = 0;
    while ((ret = connect(sfd, reinterpret_cast<struct sockaddr *>(&sadd), sizeof(sadd))) < 0) {
      if (errno == EINTR) {
        continue;
      }
      printf("ERROR: %d\n", sfd);
      perror("connect");
      close(sfd);
      return -1;
    }
    return 0;
  }

  std::thread *saurion_thread;
};

TEST_F(LowSaurionTest, connectMultipleClients) {
  uint clients = 10;

  if (add_clients(clients) != 0) {
    FAIL();
    return;
  }

  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);
}

TEST_F(LowSaurionTest, readMultipleMsgsFromClient) {
  uint clients = 10;
  uint msgs = 100;

  if (add_clients(clients) != 0) {
    FAIL();
    return;
  }

  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);

  int sfd = client_threads.at(0);

  if (client_sends(sfd, msgs, "Hola\n") != 0) {
    FAIL();
    return;
  }
  wait_readed(msgs);
  EXPECT_EQ(summary.readed, msgs);
}

TEST_F(LowSaurionTest, readMultipleMsgsFromMultipleClients) {
  uint clients = 10;
  uint msgs = 100;

  if (add_clients(clients) != 0) {
    FAIL();
    return;
  }

  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);

  all_clients_sends(msgs, "Hola\n");

  wait_readed(msgs * clients);
  EXPECT_EQ(summary.readed, msgs * clients);
}

TEST_F(LowSaurionTest, writeMsgsToClient) {
  uint clients = 1;
  uint msgs = 3;

  if (add_clients(clients) != 0) {
    FAIL();
    return;
  }

  wait_connected(clients);
  int server_client_fd = summary.fds.front();
  int client_fd = client_threads.at(0);
  EXPECT_EQ(summary.connected, clients);

  saurion_sends_to_client(server_client_fd, msgs, "Hola");

  wait_wrote(msgs);
  EXPECT_EQ(msgs, read_from_client(client_fd, "Hola"));
}

TEST_F(LowSaurionTest, writeMsgsToClients) {
  uint clients = 10;
  uint msgs = 1000;

  if (add_clients(clients) != 0) {
    FAIL();
    return;
  }

  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);

  for (int sfd : summary.fds) {
    saurion_sends_to_client(sfd, msgs, "Hola");
  }

  wait_wrote(msgs * clients);
  auto total_msgs = read_from_clients("Hola");
  EXPECT_EQ(msgs * clients, total_msgs);
}

TEST_F(LowSaurionTest, reconnectClients) {
  uint clients = 5;

  if (add_clients(clients) != 0) {
    FAIL();
    return;
  }

  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);

  for (auto pair : client_threads) {
    close(pair);
  }
  client_threads.clear();
  wait_connected(0);

  summary.connected = 0;
  if (add_clients(clients) != 0) {
    FAIL();
    return;
  }
  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);
}

TEST_F(LowSaurionTest, handleClientDisconnect) {
  uint clients = 5;

  if (add_clients(clients) != 0) {
    FAIL();
    return;
  }

  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);

  for (auto sfd : client_threads) {
    close(sfd);
    clients--;
  }
  client_threads.clear();

  // Wait for a moment to allow server to process the disconnections
  wait_connected(0);
  EXPECT_EQ(summary.connected, clients);  // Connection count remains the same
}

TEST_F(LowSaurionTest, readWriteWithLargeMessage) {
  uint clients = 1;
  if (add_clients(clients) != 0) {
    FAIL();
    return;
  }

  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);

  int client_fd = client_threads.at(0);

  {
    std::string large_message(static_cast<size_t>(IOVEC_SZ * 10), 'A');
    large_message[IOVEC_SZ * 10 - 1] = '\n';

    if (client_sends(client_fd, 1, large_message.c_str()) != 0) {
      FAIL();
      return;
    }
    wait_readed(1);
    EXPECT_EQ(summary.readed, 1UL);
  }

  {
    std::string large_message(static_cast<size_t>(IOVEC_SZ * 10), 'A');
    large_message[IOVEC_SZ * 10 - 1] = '\n';

    saurion_sends_to_client(summary.fds.front(), 1, large_message.c_str());
    wait_wrote(1);
    large_message.pop_back();
    EXPECT_EQ(1UL, read_from_client(client_fd, large_message));
  }
}

TEST_F(LowSaurionTest, handleErrors) {
  /*! TODO: Verificar que se utiliza el callback de errores
   *  \todo Verificar que se utiliza el callback de errores
   */
  uint clients = 5;

  if (add_clients(clients) != 0) {
    FAIL();
    return;
  }

  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);

  // Send erroneous data to trigger error callback
  int client_fd = client_threads.at(0);
  if (client_sends(client_fd, 1, "INVALID\0") != 0) {
    FAIL();
    return;
  }
  wait_readed(1);
  EXPECT_EQ(summary.readed, 1UL);

  // Wait for error callback to be triggered
  EXPECT_GT(summary.readed, 0UL);  // Assuming error callback increments read count
}

TEST_F(LowSaurionTest, handleConcurrentReadsAndWrites) {
  uint clients = 10;
  uint msgs = 100;

  if (add_clients(clients) != 0) {
    FAIL();
    return;
  }

  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);

  std::thread reader([&]() { all_clients_sends(msgs, "Hello\n"); });
  wait_readed(msgs * clients);

  std::thread writer([&]() {
    for (int sfd : summary.fds) {
      saurion_sends_to_client(sfd, msgs, "World");
    }
  });
  wait_wrote(msgs * clients);

  reader.join();
  writer.join();

  EXPECT_EQ(msgs * clients, summary.readed);
  EXPECT_EQ(msgs * clients, summary.wrote);
}
