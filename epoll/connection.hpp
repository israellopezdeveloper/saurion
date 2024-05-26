#ifndef CONNECTION_H
#define CONNECTION_H

#include <pthread.h>
#include <unistd.h>

#include <queue>

class Connection {
 public:
  explicit Connection(int nfd) noexcept;
  ~Connection() noexcept;
  Connection(const Connection &) = delete;
  Connection &operator=(const Connection &) = delete;
  Connection(Connection &&) = delete;
  Connection &operator=(Connection &&) = delete;

  void read(char *&buffer, ssize_t &len);
  bool write() noexcept;
  void send(const char *data, size_t len) noexcept;
  void close() const noexcept;

  int fd() const noexcept;

 private:
  using MsgQueue = std::queue<std::pair<char *, const ssize_t> *>;
  int m_fd;
  char *m_previous;
  size_t m_previous_len;
  MsgQueue m_messages;
  pthread_mutex_t m_queue_mtx;
};

#endif  // CONNECTION_H
