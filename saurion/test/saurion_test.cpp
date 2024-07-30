#include <fcntl.h>      // for open, O_WRONLY
#include <pthread.h>    // for pthread_mutex_lock, pthread_mutex_unlock
#include <sys/stat.h>   // for mkfifo
#include <sys/types.h>  // for ssize_t, pid_t
#include <sys/wait.h>   // for waitpid
#include <time.h>       // for time
#include <unistd.h>     // for dup2, close, execvp, fork, unlink, STDER...

#include <algorithm>         // for remove
#include <csignal>           // for size_t, signal, SIGINT
#include <cstdint>           // for uint32_t
#include <cstdio>            // for fflush, fprintf, perror, remove, FILE, NULL
#include <cstdlib>           // for exit, free, malloc, rand, srand
#include <cstring>           // for memset, strcpy
#include <filesystem>        // for directory_iterator, path, directory_entry
#include <fstream>           // for basic_ostream, operator<<, endl, basic_i...
#include <initializer_list>  // for initializer_list
#include <iostream>          // for cout, cerr
#include <regex>             // for regex_match, regex
#include <sstream>           // for basic_stringstream
#include <string>            // for char_traits, basic_string, allocator
#include <thread>            // for thread
#include <vector>            // for vector

#include "config.h"       // for ERROR_CODE, LOG_END, LOG_INIT, CHUNK_SZ
#include "gtest/gtest.h"  // for Message, EXPECT_EQ, TestPartResult, Test...
#include "low_saurion.h"  // for saurion, saurion_send, EXTERNAL_set_socket

#define PORT 8080

#define SCRIPT "./scripts/client"
#define FIFO "/tmp/saurion_test_fifo.XXX"
#define FIFO_LENGTH 27

struct summary {
  explicit summary() = default;
  ~summary() = default;
  summary(const summary &) = delete;
  summary(summary &&) = delete;
  summary &operator=(const summary &) = delete;
  summary &operator=(summary &&) = delete;
  uint32_t connected = 0;
  std::vector<int> fds;
  pthread_cond_t connected_c = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t connected_m = PTHREAD_MUTEX_INITIALIZER;
  uint32_t disconnected = 0;
  pthread_cond_t disconnected_c = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t disconnected_m = PTHREAD_MUTEX_INITIALIZER;
  size_t readed = 0;
  pthread_cond_t readed_c = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t readed_m = PTHREAD_MUTEX_INITIALIZER;
  uint32_t wrote = 0;
  pthread_cond_t wrote_c = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t wrote_m = PTHREAD_MUTEX_INITIALIZER;
} __attribute__((aligned(128))) summary;

void signalHandler(int signum);

static pid_t pid = 0;

class LowSaurionTest : public ::testing::Test {
 public:
  struct saurion *saurion;
  static std::thread *sender;
  static char *fifo_name;
  static FILE *fifo_write;

 protected:
  static char *generate_random_fifo_name() {
    fifo_name = (char *)malloc(FIFO_LENGTH);
    if (!fifo_name) {
      return NULL;  // Handle error
    }

    strcpy(fifo_name, FIFO);
    // Seed the random number generator
    srand(time(NULL));

    // Generate the random "XXX" string
    for (int i = 23; i < 26; i++) {
      char c = (rand() % 10) + '0';
      fifo_name[i] = c;
    }

    return fifo_name;
  }

  static void SetUpTestSuite() {
    fifo_name = generate_random_fifo_name();
    std::signal(SIGINT, signalHandler);
    if (!fifo_name) {
      // Handle error generating random name
      exit(ERROR_CODE);
    }
    if (mkfifo(fifo_name, 0666) == -1) {
      free(fifo_name);
      exit(ERROR_CODE);
    }
    sender = new std::thread([=]() {
      pid = fork();
      if (pid < 0) {
        return ERROR_CODE;
      }
      if (pid == 0) {
        std::vector<const char *> exec_args;
        for (char *item : {(char *)SCRIPT, (char *)"-p", fifo_name}) {
          exec_args.push_back(item);
        }
        exec_args.push_back(nullptr);
        int dev_null = open("/dev/null", O_WRONLY);
        if (dev_null == -1) {
          perror("Failed to open /dev/null");
          exit(ERROR_CODE);
        }

        // Redirige stdout a /dev/null
        if (dup2(dev_null, STDOUT_FILENO) == -1) {
          perror("Failed to redirect stdout");
          exit(ERROR_CODE);
        }

        // Redirige stderr a /dev/null
        if (dup2(dev_null, STDERR_FILENO) == -1) {
          perror("Failed to redirect stderr");
          exit(ERROR_CODE);
        }

        close(dev_null);

        // Ejecuta el comando y ignora el retorno
        execvp(SCRIPT, const_cast<char *const *>(exec_args.data()));
      }
      int status;
      waitpid(pid, &status, 0);
      return SUCCESS_CODE;
    });
    fifo_write = fopen(fifo_name, "w");
  }

  static void TearDownTestSuite() {
    close_clients();
    if (sender != nullptr) {
      if (sender->joinable()) {
        sender->join();
      }
      delete sender;
    }
    fclose(fifo_write);
    unlink(fifo_name);
    free(fifo_name);
  }

  void SetUp() override {
    LOG_INIT("");
    summary.connected = 0;
    summary.disconnected = 0;
    summary.readed = 0;
    summary.wrote = 0;
    summary.fds.clear();
    saurion = saurion_create(3);
    if (!saurion) {
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
    saurion->cb.on_readed = [](int, const void *const, const ssize_t size, void *) -> void {
      pthread_mutex_lock(&summary.readed_m);
      summary.readed += size;
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
      pthread_mutex_lock(&summary.disconnected_m);
      summary.disconnected++;
      pthread_cond_signal(&summary.disconnected_c);
      pthread_mutex_unlock(&summary.disconnected_m);
      pthread_mutex_lock(&summary.connected_m);
      auto &vec = summary.fds;
      vec.erase(std::remove(vec.begin(), vec.end(), sfd), vec.end());
      pthread_cond_signal(&summary.connected_c);
      pthread_mutex_unlock(&summary.connected_m);
    };
    saurion->cb.on_error = [](int, const char *const, const ssize_t, void *) -> void {};
    if (!saurion_start(saurion)) {
      exit(ERROR_CODE);
    }
    pthread_mutex_lock(&saurion->status_m);
    while (saurion->status != 1) {
      pthread_cond_wait(&saurion->status_c, &saurion->status_m);
    }
    pthread_mutex_unlock(&saurion->status_m);
    LOG_END("");
  }

  void TearDown() override {
    LOG_INIT("");
    disconnect_clients();
    saurion_stop(saurion);
    saurion_destroy(saurion);
    deleteLogFiles();
    LOG_END("");
  }

  static void wait_connected(uint32_t n) {
    pthread_mutex_lock(&summary.connected_m);
    while (summary.connected != n) {
      pthread_cond_wait(&summary.connected_c, &summary.connected_m);
    }
    pthread_mutex_unlock(&summary.connected_m);
  }

  static void wait_disconnected(uint32_t n) {
    pthread_mutex_lock(&summary.disconnected_m);
    while (summary.disconnected != n) {
      pthread_cond_wait(&summary.disconnected_c, &summary.disconnected_m);
    }
    pthread_mutex_unlock(&summary.disconnected_m);
  }

  static void wait_readed(size_t n) {
    pthread_mutex_lock(&summary.readed_m);
    while (summary.readed < n) {
      pthread_cond_wait(&summary.readed_c, &summary.readed_m);
    }
    pthread_mutex_unlock(&summary.readed_m);
  }

  static void wait_wrote(uint32_t n) {
    pthread_mutex_lock(&summary.wrote_m);
    while (summary.wrote < n) {
      pthread_cond_wait(&summary.wrote_c, &summary.wrote_m);
    }
    pthread_mutex_unlock(&summary.wrote_m);
  }

  static void connect_clients(uint32_t n) {
    fprintf(fifo_write, "connect;%d\n", n);
    fflush(fifo_write);  // Asegurarse de que el buffer se escribe en el FIFO
  }

  static void disconnect_clients() {
    fprintf(fifo_write, "disconnect\n");
    fflush(fifo_write);  // Asegurarse de que el buffer se escribe en el FIFO
  }

  static void clients2saurion(uint32_t n, const char *const msg, uint32_t delay) {
    fprintf(fifo_write, "send;%d;%s;%d\n", n, msg, delay);
    fflush(fifo_write);  // Asegurarse de que el buffer se escribe en el FIFO
  }

  static void close_clients() {
    fprintf(fifo_write, "close\n");
    fflush(fifo_write);  // Asegurarse de que el buffer se escribe en el FIFO
  }

  void saurion_sends_to_client(int sfd, uint32_t n, const char *const msg) const {
    for (uint32_t i = 0; i < n; ++i) {
      saurion_send(saurion, sfd, msg);
    }
  }

  void saurion_sends_to_all_clients(uint32_t n, const char *const msg) {
    for (auto sfd : summary.fds) {
      for (uint32_t i = 0; i < n; ++i) {
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

  static size_t read_from_clients(const std::string &search) {
    size_t occurrences = 0;
    for (const auto &entry : std::filesystem::directory_iterator("/tmp/")) {
      const auto &path = entry.path();
      if (path.filename().string().find("saurion_sender.") == 0) {
        std::ifstream file(path);
        if (file.is_open()) {
          std::stringstream buffer;
          buffer << file.rdbuf();
          std::string content = buffer.str();
          occurrences += countOccurrences(content, search);
        }
      }
    }
    return occurrences;
  }

 private:
  void deleteLogFiles() {
    for (const auto &entry : std::filesystem::directory_iterator("/tmp/")) {
      const auto &path = entry.path();
      if (path.filename().string().find("saurion_sender.") == 0) {
        try {
          std::filesystem::remove(path);
        } catch (...) {
        }
      }
    }
  }
};

char *LowSaurionTest::fifo_name = nullptr;
std::thread *LowSaurionTest::sender = nullptr;
FILE *LowSaurionTest::fifo_write = nullptr;

void signalHandler(int) {
  if (kill(pid, SIGINT) == -1) {
    exit(EXIT_FAILURE);
  } else {
  }

  // Intenta eliminar el archivo FIFO
  std::remove(LowSaurionTest::fifo_name);
  std::regex pattern("^saurion_sender.*\\.log$");

  for (const auto &entry : std::filesystem::directory_iterator("/tmp")) {
    if (std::filesystem::is_regular_file(entry)) {
      std::string filename = entry.path().filename().string();
      if (std::regex_match(filename, pattern)) {
        try {
          std::filesystem::remove(entry.path());
        } catch (const std::filesystem::filesystem_error &e) {
        }
      }
    }
  }
  // Termina el programa
  exit(ERROR_CODE);
}

TEST_F(LowSaurionTest, connectMultipleClients) {
  uint32_t clients = 10;
  connect_clients(clients);
  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);
  disconnect_clients();
  wait_disconnected(clients);
  EXPECT_EQ(summary.disconnected, clients);
}

TEST_F(LowSaurionTest, readMultipleMsgsFromClients) {
  LOG_INIT("");
  uint32_t clients = 20;
  uint32_t msgs = 100;
  connect_clients(clients);
  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);
  clients2saurion(msgs, "Hola", 0);
  wait_readed(msgs * clients * 4);
  EXPECT_EQ(summary.readed, msgs * clients * 4);
  disconnect_clients();
  wait_disconnected(clients);
  EXPECT_EQ(summary.disconnected, clients);
  LOG_END("");
}

TEST_F(LowSaurionTest, writeMsgsToClients) {
  uint32_t clients = 20;
  uint32_t msgs = 100;
  connect_clients(clients);
  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);
  for (auto &cfd : summary.fds) {
    saurion_sends_to_client(cfd, msgs, "Hola");
  }
  wait_wrote(msgs * clients);
  disconnect_clients();
  wait_disconnected(clients);
  EXPECT_EQ(msgs * clients, read_from_clients("Hola"));
  EXPECT_EQ(summary.disconnected, clients);
}

TEST_F(LowSaurionTest, reconnectClients) {
  uint32_t clients = 5;
  connect_clients(clients);
  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);
  disconnect_clients();
  wait_disconnected(clients);
  EXPECT_EQ(summary.disconnected, clients);
  connect_clients(clients);
  wait_connected(clients * 2);
  EXPECT_EQ(summary.connected, clients * 2);
  disconnect_clients();
  wait_disconnected(clients * 2);
  EXPECT_EQ(summary.disconnected, clients * 2);
}

TEST_F(LowSaurionTest, readWriteWithLargeMessage) {
  uint32_t clients = 1;
  size_t size = CHUNK_SZ * 10;
  char *str = new char[size + 1];
  memset(str, 'A', size);
  str[size - 1] = '1';
  str[size] = '\0';
  connect_clients(clients);
  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);
  clients2saurion(1, str, 0);
  wait_readed(size);
  EXPECT_EQ(summary.readed, 81920UL);
  saurion_sends_to_client(summary.fds.front(), 1, (char *)str);
  wait_wrote(1);
  disconnect_clients();
  wait_disconnected(clients);
  EXPECT_EQ(1UL, read_from_clients((char *)str));
  EXPECT_EQ(summary.disconnected, clients);
  delete[] str;
}

TEST_F(LowSaurionTest, handleConcurrentReadsAndWrites) {
  uint32_t clients = 20;
  uint32_t msgs = 100;
  connect_clients(clients);
  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);
  clients2saurion(msgs, "Hola", 2);
  saurion_sends_to_all_clients(msgs, "Hola");
  wait_readed(msgs * clients * 4);
  EXPECT_EQ(msgs * clients * 4, summary.readed);
  wait_wrote(msgs * clients);
  EXPECT_EQ(msgs * clients, summary.wrote);
  disconnect_clients();
  wait_disconnected(clients);
}

TEST_F(LowSaurionTest, largeNumberOfMessages) {
  uint32_t clients = 10;
  uint32_t msgs = 1000;
  connect_clients(clients);
  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);
  clients2saurion(msgs, "Test Message", 0);
  wait_readed(msgs * clients * 12);
  EXPECT_EQ(summary.readed, msgs * clients * 12);
  disconnect_clients();
  wait_disconnected(clients);
  EXPECT_EQ(summary.disconnected, clients);
}

TEST_F(LowSaurionTest, handleInvalidMessages) {
  uint32_t clients = 10;
  connect_clients(clients);
  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);
  // Send invalid messages
  clients2saurion(clients, "", 0);       // Empty message
  clients2saurion(clients, nullptr, 0);  // Null message
  disconnect_clients();
  wait_disconnected(clients);
  EXPECT_EQ(summary.disconnected, clients);
}

TEST_F(LowSaurionTest, stressTest) {
  const char *str = "Stress Test Message";
  uint32_t clients = 50;
  uint32_t msgs = 1000;
  connect_clients(clients);
  wait_connected(clients);
  EXPECT_EQ(summary.connected, clients);
  clients2saurion(msgs, str, 1);
  wait_readed(msgs * clients *
              strlen(str));  // Adjust the expected length based on your message size
  EXPECT_EQ(summary.readed, msgs * clients * strlen(str));
  disconnect_clients();
  wait_disconnected(clients);
  EXPECT_EQ(summary.disconnected, clients);
}
