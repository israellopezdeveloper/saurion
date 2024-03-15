// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "empollon.hpp"

#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>

#include <stdexcept>

#include "connection.hpp"

using E = Empollon;

E::Empollon(const int fd, const bool svr) noexcept
    : m_epoll_fd(epoll_create1(0)),
      m_socket(new Connection(fd)),
      m_event(new Connection(eventfd(0, EFD_NONBLOCK))),
      m_is_server(svr) {
  if (m_epoll_fd == -1) {
    return;
  }
  if (m_event->fd() == -1) {
    return;
  }
  try {
    add(m_event);
  } catch (...) {
  }
  try {
    add(m_socket);
  } catch (const std::runtime_error &e) {
    delete m_socket;
  }
}
E::~Empollon() noexcept {
  stop();
  close(m_epoll_fd);
  close(m_event->fd());
  close(m_socket->fd());
  delete m_event;
  delete m_socket;
}

Connection *const E::socket() const noexcept { return m_socket; }

void E::on_connected(ConnectedCb cb, void *arg) noexcept {
  m_connected_cb = cb;
  m_connected_arg = arg;
}
void E::on_readed(ReadedCb cb, void *arg) noexcept {
  m_readed_cb = cb;
  m_readed_arg = arg;
}
void E::on_wrote(WroteCb cb, void *arg) noexcept {
  m_wrote_cb = cb;
  m_wrote_arg = arg;
}
void E::on_closed(ClosedCb cb, void *arg) noexcept {
  m_closed_cb = cb;
  m_closed_arg = arg;
}

void E::add(Connection *conn) {
  struct epoll_event event;
  event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR | EPOLLONESHOT;
  event.data.ptr = conn;

  if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, conn->fd(), &event) == -1) {
    throw std::runtime_error("epoll_ctl");
  }
}

void E::modify(Connection *conn) noexcept {
  struct epoll_event event;
  event.events = EPOLLHUP | EPOLLERR | EPOLLONESHOT;
  if (conn->is_readable() || conn == m_event) {
    event.events |= EPOLLIN;
  }
  if (conn->is_writable() || conn == m_event) {
    event.events |= EPOLLOUT;
  }
  event.data.ptr = conn;

  epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, conn->fd(), &event);
}
void E::remove(Connection *conn) noexcept {
  // Manejar eventos de cierre o error
  if (m_closed_cb) {
    m_closed_cb(conn->fd(), m_closed_arg);
  }
  // Eliminar el descriptor de archivo del conjunto epoll
  epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, conn->fd(), nullptr);
  delete conn;
}

void E::wait() noexcept {
  struct epoll_event events[MAX_EVENTS];
  int num_ready = 0;
  while (m_stop_f == 0) {
    num_ready = epoll_wait(m_epoll_fd, events, MAX_EVENTS, -1);
    if (num_ready == -1) {
      if (errno != EINTR) {
        break;
      }
      continue;
    }

    for (int i = 0; i < num_ready; ++i) {
      Connection *conn = reinterpret_cast<Connection *>(events[i].data.ptr);
      if (events[i].events & (EPOLLHUP | EPOLLERR)) {
        Connection *conn = reinterpret_cast<Connection *>(events[i].data.ptr);
        remove(conn);
        continue;
      }
      if (events[i].events & EPOLLIN) {
        if (conn == m_socket && m_is_server) {
          connect_new();
        } else {
          if (!read_fd(conn)) continue;
        }
      }
      if (events[i].events & EPOLLOUT) {
        write_fd(conn);
      }

      try {
        modify(conn);
      } catch (...) {
      }
    }
  }
}
void E::stop() noexcept {
  m_stop_f = 1;
  eventfd_write(m_event->fd(), 1);
}

void E::connect_new() noexcept {
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  int client_fd = accept(m_socket->fd(), (struct sockaddr *)&client_addr, &client_addr_len);
  if (client_fd == -1) {
    return;
  }
  auto *conn = new Connection(client_fd);
  try {
    add(conn);
    if (m_connected_cb) {
      m_connected_cb(*conn, m_connected_arg);
    }
  } catch (const std::runtime_error &e) {
    delete conn;
  }
}
bool E::read_fd(Connection *conn) noexcept {
  char buffer[1024];
  char *buffer_ptr = buffer;
  ssize_t bytes_read = sizeof(buffer);
  conn->read(buffer_ptr, bytes_read);
  if (bytes_read < 1) {
    if (errno != EAGAIN) {
      remove(conn);
    }
    return false;
  }
  if (m_readed_cb) {
    m_readed_cb(conn->fd(), buffer, bytes_read, m_readed_arg);
  }
  return true;
}
void E::write_fd(Connection *conn) noexcept {
  if (!conn->write()) {
    remove(conn);
    return;
  }
  if (m_wrote_cb) {
    m_wrote_cb(conn->fd(), m_wrote_arg);
  }
}
