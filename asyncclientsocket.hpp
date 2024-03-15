#ifndef ASYNC_CLIENT_SOCKET_H
#define ASYNC_CLIENT_SOCKET_H

#include "connection.hpp"
#include "empollon.hpp"

class AsyncClientSocket {
 public:
  using ConnectedCb = Empollon::ConnectedCb;
  using ReadedCb = Empollon::ReadedCb;
  using WroteCb = Empollon::WroteCb;
  using ClosedCb = Empollon::ClosedCb;

  explicit AsyncClientSocket() noexcept;
  ~AsyncClientSocket() noexcept;
  AsyncClientSocket(const AsyncClientSocket &) = delete;
  AsyncClientSocket &operator=(const AsyncClientSocket &) = delete;

  void on_connected(ConnectedCb cb, void *arg) noexcept;
  void on_readed(ReadedCb cb, void *arg) noexcept;
  void on_wrote(WroteCb cb, void *arg) noexcept;
  void on_closed(ClosedCb cb, void *arg) noexcept;

  void connect(const char *ip, const int port);
  void stop() noexcept;
  void send(const char *data, const size_t len) noexcept;

 private:
  int m_client_fd;
  Connection *m_client_conn;
  Empollon m_epoll;
  pthread_t m_client_th;

  ConnectedCb m_connected_cb = nullptr;
  void *m_connected_arg = nullptr;

  static void *main_th(void *arg) noexcept;
};

#endif  // ASYNC_CLIENT_SOCKET_H
