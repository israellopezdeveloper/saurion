#ifndef ASYNC_CLIENT_SOCKET_H
#define ASYNC_CLIENT_SOCKET_H

#include "../epoll/empollon.hpp"

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
  AsyncClientSocket(AsyncClientSocket &&) = delete;
  AsyncClientSocket &operator=(AsyncClientSocket &&) = delete;

  void on_connected(ConnectedCb ncb, void *arg) noexcept;
  void on_readed(ReadedCb ncb, void *arg) noexcept;
  void on_wrote(WroteCb ncb, void *arg) noexcept;
  void on_closed(ClosedCb ncb, void *arg) noexcept;

  void connect(const char *ipa, int port);
  void stop() noexcept;
  void send(const char *data, size_t len) noexcept;

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
