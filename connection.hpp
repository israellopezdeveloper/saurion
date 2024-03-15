#ifndef CONNECTION_H
#define CONNECTION_H

#include <pthread.h>
#include <unistd.h>

#include <queue>

class Connection {
 public:
  explicit Connection(const int fd) noexcept;
  ~Connection() noexcept;
  Connection(const Connection &) = delete;
  Connection &operator=(const Connection &) = delete;

  void read(char *&buffer, ssize_t &len) noexcept;
  bool write() noexcept;
  void send(const char *data, const size_t len) noexcept;
  void close() noexcept;

  int fd() const noexcept;
  bool is_readable() const noexcept;
  bool is_writable() const noexcept;

 private:
  using MsgQueue = std::queue<std::pair<const char *, const ssize_t> *>;
  int m_fd;
  int m_availability;
  int m_status;
  MsgQueue m_messages;
  pthread_mutex_t m_queue_mtx;

  void readable(bool val) noexcept;
  void writeable(bool val) noexcept;

  bool is_reading() const noexcept;
  bool is_writing() const noexcept;
  void reading(bool val) noexcept;
  void writing(bool val) noexcept;
};

#endif  // CONNECTION_H
