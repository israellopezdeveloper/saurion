// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "asyncclientsocket.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cstring>
#include <stdexcept>

using ACS = AsyncClientSocket;

ACS::AsyncClientSocket() noexcept
    : m_client_fd(socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)),
      m_client_conn(nullptr),
      m_epoll(m_client_fd, false),
      m_client_th(0) {
  m_client_conn = m_epoll.socket();
}
ACS::~AsyncClientSocket() noexcept {
  m_epoll.stop();
  if (m_client_th != 0U) {
    pthread_join(m_client_th, nullptr);
    m_client_th = 0;
  }
  if (m_client_fd != -1) {
    close(m_client_fd);
    m_client_fd = -1;
  }
}

void ACS::on_connected(ConnectedCb ncb, void *arg) noexcept {
  m_connected_cb = ncb;
  m_connected_arg = arg;
}
void ACS::on_readed(ReadedCb ncb, void *arg) noexcept { m_epoll.on_readed(ncb, arg); }
void ACS::on_wrote(WroteCb ncb, void *arg) noexcept { m_epoll.on_wrote(ncb, arg); }
void ACS::on_closed(ClosedCb ncb, void *arg) noexcept { m_epoll.on_closed(ncb, arg); }

void ACS::connect(const char *ipa, const int port) {
  if (m_client_fd == -1) {
    throw std::runtime_error("socket failed");
  }
  struct timeval timeout {};
  timeout.tv_sec = 1;
  if (setsockopt(m_client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
    throw std::runtime_error("client socket failed 1");
  }
  if (setsockopt(m_client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1) {
    throw std::runtime_error("client socket failed 2");
  }
  struct sockaddr_in c_addr {};
  c_addr.sin_family = AF_INET;
  c_addr.sin_port = htons(port);
  c_addr.sin_addr.s_addr = inet_addr(ipa);
  auto *ca_cast = reinterpret_cast<struct sockaddr *>(&c_addr);
  if (::connect(m_client_fd, ca_cast, sizeof(c_addr)) == -1) {
    if (errno != EINPROGRESS) {
      throw std::runtime_error("connect failed");
    }
  }
  if (m_connected_cb != nullptr) {
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

void ACS::send(const char *data, const size_t len) noexcept {
  m_client_conn->send(data, len);
  m_epoll.write_fd(m_client_conn);
}

void *ACS::main_th(void *arg) noexcept {
  ACS *client = static_cast<AsyncClientSocket *>(arg);
  client->m_epoll.wait();
  return nullptr;
}
