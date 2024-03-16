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

  void read(char *&buffer, ssize_t &len) noexcept;
  bool write() noexcept;
  void send(const char *data, size_t len) noexcept;
  void close() const noexcept;

  int fd() const noexcept;
  bool is_readable() const noexcept;
  bool is_writable() const noexcept;

  void wait_not_using() noexcept;

 private:
  using MsgQueue = std::queue<std::pair<char *, const ssize_t> *>;
  int m_fd;
  int m_availability;
  int m_status;
  MsgQueue m_messages;
  pthread_mutex_t m_queue_mtx;
  pthread_mutex_t m_usable_mtx;
  pthread_cond_t m_usable_cond;

  void readable(bool val) noexcept;
  void writeable(bool val) noexcept;

  bool is_reading() const noexcept;
  bool is_writing() const noexcept;
  void reading(bool val) noexcept;
  void writing(bool val) noexcept;
};

#endif  // CONNECTION_H
