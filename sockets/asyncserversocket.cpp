// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "asyncserversocket.hpp"

#include <netinet/in.h>

#include <cstring>
#include <stdexcept>

#define MAX_EVENTS 10

using ASS = AsyncServerSocket;

ASS::AsyncServerSocket() noexcept
    : m_server_fd(socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)),
      m_epoll(m_server_fd, true),
      m_server_th(0) {}
ASS::~AsyncServerSocket() noexcept {
  stop();
  if (m_server_fd != -1) {
    close(m_server_fd);
  }
}

void ASS::on_connected(ConnectedCb ncb, void *arg) noexcept { m_epoll.on_connected(ncb, arg); }
void ASS::on_readed(ReadedCb ncb, void *arg) noexcept { m_epoll.on_readed(ncb, arg); }
void ASS::on_wrote(WroteCb ncb, void *arg) noexcept { m_epoll.on_wrote(ncb, arg); }
void ASS::on_closed(ClosedCb ncb, void *arg) noexcept { m_epoll.on_closed(ncb, arg); }

void ASS::start(const int port) {
  if (m_server_fd == -1) {
    throw std::runtime_error("socket failed");
  }
  struct timeval timeout {};
  timeout.tv_sec = 1;
  if (setsockopt(m_server_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
    throw std::runtime_error(strerror(errno));
  }
  if (setsockopt(m_server_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1) {
    throw std::runtime_error(strerror(errno));
  }
  int opt = 1;
  if (setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
    throw std::runtime_error("setsockopt failed");
  }
  struct sockaddr_in s_addr {};
  s_addr.sin_family = AF_INET;
  s_addr.sin_addr.s_addr = INADDR_ANY;
  s_addr.sin_port = htons(port);
  auto *sa_cast = reinterpret_cast<struct sockaddr *>(&s_addr);
  if (bind(m_server_fd, sa_cast, sizeof(s_addr)) == -1) {
    throw std::runtime_error("bind failed");
  }
  if (listen(m_server_fd, SOMAXCONN) == -1) {
    throw std::runtime_error("listen failed");
  }
  if (pthread_create(&m_server_th, nullptr, &ASS::main_th, this) != 0) {
    throw std::runtime_error("server thread creation failed");
  }
}
void ASS::stop() noexcept {
  m_epoll.stop();
  if (m_server_th != 0U) {
    pthread_join(m_server_th, nullptr);
    m_server_th = 0;
  }
  if (m_server_fd != -1) {
    close(m_server_fd);
    m_server_fd = -1;
  }
}

void ASS::send(Connection *conn, char *data, const size_t len) noexcept {
  conn->send(data, len);
  m_epoll.write_fd(conn);
}

void *ASS::main_th(void *arg) noexcept {
  ASS *server = static_cast<AsyncServerSocket *>(arg);
  server->m_epoll.wait();
  return nullptr;
}
