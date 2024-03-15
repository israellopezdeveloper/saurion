// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "asyncclientsocket.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <stdexcept>

using ACS = AsyncClientSocket;

ACS::AsyncClientSocket() noexcept
    : m_client_fd(socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)),
      m_client_conn(nullptr),
      m_client_th(0),
      m_epoll(m_client_fd, false) {
  m_client_conn = m_epoll.socket();
}
ACS::~AsyncClientSocket() noexcept {
  m_epoll.stop();
  if (m_client_th) {
    pthread_join(m_client_th, nullptr);
    m_client_th = 0;
  }
  if (m_client_fd != -1) {
    close(m_client_fd);
    m_client_fd = -1;
  }
}

void ACS::on_connected(ConnectedCb cb, void *arg) noexcept {
  m_connected_cb = cb;
  m_connected_arg = arg;
}
void ACS::on_readed(ReadedCb cb, void *arg) noexcept { m_epoll.on_readed(cb, arg); }
void ACS::on_wrote(WroteCb cb, void *arg) noexcept { m_epoll.on_wrote(cb, arg); }
void ACS::on_closed(ClosedCb cb, void *arg) noexcept { m_epoll.on_closed(cb, arg); }

void ACS::connect(const char *ip, const int port) {
  if (m_client_fd == -1) {
    throw std::runtime_error("socket failed");
  }
  struct sockaddr_in client_addr;
  client_addr.sin_family = AF_INET;
  client_addr.sin_port = htons(port);
  client_addr.sin_addr.s_addr = inet_addr(ip);
  if (::connect(m_client_fd, (struct sockaddr *)&client_addr, sizeof(client_addr)) == -1) {
    if (errno != EINPROGRESS) {
      throw std::runtime_error("connect failed");
    }
  }
  if (m_connected_cb) {
    m_connected_cb(*m_epoll.socket(), m_connected_arg);
  }
  if (pthread_create(&m_client_th, nullptr, &ACS::main_th, this) != 0) {
    throw std::runtime_error("client thread creation failed");
  }
}
void ACS::stop() noexcept {
  m_epoll.stop();
  if (m_client_fd != -1) {
    close(m_client_fd);
    m_client_fd = -1;
  }
}

void ACS::send(const char *data, const size_t len) noexcept { m_client_conn->send(data, len); }

void *ACS::main_th(void *arg) noexcept {
  ACS *client = static_cast<AsyncClientSocket *>(arg);
  client->m_epoll.wait();
  return nullptr;
}
