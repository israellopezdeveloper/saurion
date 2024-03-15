#ifndef EMPOLLON_H
#define EMPOLLON_H

#include <bits/types/sig_atomic_t.h>

#include "connection.hpp"

#define MAX_EVENTS 10

class Empollon {
 public:
  explicit Empollon(const int fd, const bool svr) noexcept;
  ~Empollon() noexcept;
  Empollon(const Empollon &) = delete;
  Empollon &operator=(const Empollon &) = delete;

  using ConnectedCb = void (*)(Connection &, void *);
  using ReadedCb = void (*)(int, char *, ssize_t, void *);
  using WroteCb = void (*)(int, void *);
  using ClosedCb = void (*)(int, void *);

  Connection *const socket() const noexcept;

  void on_connected(ConnectedCb cb, void *arg) noexcept;
  void on_readed(ReadedCb cb, void *arg) noexcept;
  void on_wrote(WroteCb cb, void *arg) noexcept;
  void on_closed(ClosedCb cb, void *arg) noexcept;

  void add(Connection *conn);

  void modify(Connection *conn) noexcept;
  void remove(Connection *conn) noexcept;

  void wait() noexcept;
  void stop() noexcept;

 private:
  int m_epoll_fd;
  Connection *m_socket;
  Connection *m_event;
  const bool m_is_server = false;
  volatile sig_atomic_t m_stop_f = 0;
  ConnectedCb m_connected_cb = nullptr;
  void *m_connected_arg = nullptr;
  ReadedCb m_readed_cb = nullptr;
  void *m_readed_arg = nullptr;
  WroteCb m_wrote_cb = nullptr;
  void *m_wrote_arg = nullptr;
  ClosedCb m_closed_cb = nullptr;
  void *m_closed_arg = nullptr;

  void connect_new() noexcept;
  bool read_fd(Connection *conn) noexcept;
  void write_fd(Connection *conn) noexcept;
};

#endif  // EMPOLLON_H
