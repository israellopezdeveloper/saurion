#ifndef EMPOLLON_H
#define EMPOLLON_H

#include <bits/types/sig_atomic_t.h>

#include <map>

#include "connection.hpp"
#include "threadpool/threadpool.hpp"

#define MAX_EVENTS 10

class Empollon {
 public:
  explicit Empollon(int nfd, bool svr);
  ~Empollon() noexcept;
  Empollon(const Empollon &) = delete;
  Empollon &operator=(const Empollon &) = delete;
  Empollon(Empollon &&) = delete;
  Empollon &operator=(Empollon &&) = delete;

  using ConnectedCb = void (*)(Connection &, void *);
  using ReadedCb = void (*)(int, char *, ssize_t, void *);
  using WroteCb = void (*)(int, void *);
  using ClosedCb = void (*)(int, void *);

  Connection *socket() const noexcept;

  void on_connected(ConnectedCb ncb, void *arg) noexcept;
  void on_readed(ReadedCb ncb, void *arg) noexcept;
  void on_wrote(WroteCb ncb, void *arg) noexcept;
  void on_closed(ClosedCb ncb, void *arg) noexcept;

  Connection *add(int nfd) const;

  void modify(Connection *conn) noexcept;
  void remove(Connection *conn) noexcept;

  void wait() noexcept;
  void stop() noexcept;

 private:
  typedef struct ThreadParam {
    Connection *conn;
    Empollon *epoll;
  } ThreadParam;
  typedef std::map<int, Connection *> Connections;

  int m_epoll_fd;
  int m_socket_fd;
  Connection *m_socket;
  Connection *m_event;
  bool m_is_server = false;
  volatile sig_atomic_t m_stop_f = 0;
  ThreadPool m_thread_pool;
  Connections m_connections;

  ConnectedCb m_connected_cb = nullptr;
  void *m_connected_arg = nullptr;
  ReadedCb m_readed_cb = nullptr;
  void *m_readed_arg = nullptr;
  WroteCb m_wrote_cb = nullptr;
  void *m_wrote_arg = nullptr;
  ClosedCb m_closed_cb = nullptr;
  void *m_closed_arg = nullptr;

  void connect_new() noexcept;
  void read_fd(Connection *conn) noexcept;

 public:
  void write_fd(Connection *conn) noexcept;
};

#endif  // EMPOLLON_H
