#ifndef ASYNC_SERVER_SOCKET_H
#define ASYNC_SERVER_SOCKET_H

#include "connection.hpp"
#include "empollon.hpp"

class AsyncServerSocket {
 public:
  using ConnectedCb = Empollon::ConnectedCb;
  using ReadedCb = Empollon::ReadedCb;
  using WroteCb = Empollon::WroteCb;
  using ClosedCb = Empollon::ClosedCb;

  explicit AsyncServerSocket() noexcept;
  ~AsyncServerSocket() noexcept;
  AsyncServerSocket(const AsyncServerSocket &) = delete;
  AsyncServerSocket &operator=(const AsyncServerSocket &) = delete;
  AsyncServerSocket(AsyncServerSocket &&) = delete;
  AsyncServerSocket &operator=(AsyncServerSocket &&) = delete;

  void on_connected(ConnectedCb ncb, void *arg) noexcept;
  void on_readed(ReadedCb ncb, void *arg) noexcept;
  void on_wrote(WroteCb ncb, void *arg) noexcept;
  void on_closed(ClosedCb ncb, void *arg) noexcept;

  void start(int port);
  void stop() noexcept;

  static void send(Connection *conn, char *data, size_t len) noexcept;

 private:
  int m_server_fd;
  Empollon m_epoll;
  pthread_t m_server_th;

  static void *main_th(void *arg) noexcept;
};

#endif  // ASYNC_SERVER_SOCKET_H
