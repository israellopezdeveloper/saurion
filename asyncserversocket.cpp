// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "asyncserversocket.hpp"

#include <netinet/in.h>

#include <stdexcept>

#define MAX_EVENTS 10

using ASS = AsyncServerSocket;

ASS::AsyncServerSocket() noexcept
    : m_server_fd(socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)),
      m_server_th(0),
      m_epoll(m_server_fd, true) {}
ASS::~AsyncServerSocket() noexcept {
  stop();
  if (m_server_fd != -1) {
    close(m_server_fd);
  }
}

void ASS::on_connected(ConnectedCb cb, void *arg) noexcept { m_epoll.on_connected(cb, arg); }
void ASS::on_readed(ReadedCb cb, void *arg) noexcept { m_epoll.on_readed(cb, arg); }
void ASS::on_wrote(WroteCb cb, void *arg) noexcept { m_epoll.on_wrote(cb, arg); }
void ASS::on_closed(ClosedCb cb, void *arg) noexcept { m_epoll.on_closed(cb, arg); }

void ASS::start(const int port) {
  if (m_server_fd == -1) {
    throw std::runtime_error("socket failed");
  }
  int opt = 1;
  if (setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
    throw std::runtime_error("setsockopt failed");
  }
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);
  if (bind(m_server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
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
  if (m_server_th) {
    pthread_join(m_server_th, nullptr);
    m_server_th = 0;
  }
  if (m_server_fd != -1) {
    close(m_server_fd);
    m_server_fd = -1;
  }
}

void ASS::send(Connection *conn, char *data, const size_t len) noexcept { conn->send(data, len); }

void *ASS::main_th(void *arg) noexcept {
  ASS *server = static_cast<AsyncServerSocket *>(arg);
  server->m_epoll.wait();
  return nullptr;
}
