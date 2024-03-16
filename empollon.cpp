// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "empollon.hpp"

#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>

#include <stdexcept>

#include "connection.hpp"

using E = Empollon;

E::Empollon(const int nfd, const bool svr)
    : m_epoll_fd(epoll_create1(0)),
      m_socket(add(nfd)),
      m_event(add(eventfd(0, EFD_NONBLOCK))),
      m_is_server(svr),
      m_thread_pool("empollon", 5) {
  if (m_epoll_fd == -1) {
    return;
  }
  if (m_event->fd() == -1) {
    return;
  }
  m_thread_pool.new_queue(m_socket->fd(), 1);
  m_thread_pool.new_queue(m_event->fd(), 1);
}
E::~Empollon() noexcept {
  stop();
  close(m_epoll_fd);
  close(m_event->fd());
  close(m_socket->fd());
  delete m_event;
  delete m_socket;
  for (auto &conn : m_connections) {
    delete conn.second;
  }
  m_thread_pool.stop();
}

Connection *E::socket() const noexcept { return m_socket; }

void E::on_connected(ConnectedCb ncb, void *arg) noexcept {
  m_connected_cb = ncb;
  m_connected_arg = arg;
}
void E::on_readed(ReadedCb ncb, void *arg) noexcept {
  m_readed_cb = ncb;
  m_readed_arg = arg;
}
void E::on_wrote(WroteCb ncb, void *arg) noexcept {
  m_wrote_cb = ncb;
  m_wrote_arg = arg;
}
void E::on_closed(ClosedCb ncb, void *arg) noexcept {
  m_closed_cb = ncb;
  m_closed_arg = arg;
}

Connection *E::add(int nfd) const {
  struct epoll_event event {};
  event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR | EPOLLONESHOT;
  event.data.ptr = new Connection(nfd);

  if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, nfd, &event) == -1) {
    throw std::runtime_error("epoll_ctl");
  }
  return static_cast<Connection *>(event.data.ptr);
}

void E::modify(Connection *conn) noexcept {
  struct epoll_event event {};
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
  try {
    m_thread_pool.add(
        conn->fd(),
        [](void *arg) {
          auto *ptr = static_cast<ThreadParam *>(arg);
          auto *conn = ptr->conn;
          auto *_this = ptr->epoll;
          if (_this->m_closed_cb != nullptr) {
            _this->m_closed_cb(conn->fd(), _this->m_closed_arg);
          }
          // Eliminar el descriptor de archivo del conjunto epoll
          epoll_ctl(_this->m_epoll_fd, EPOLL_CTL_DEL, conn->fd(), nullptr);
          delete conn;
          delete ptr;
          _this->m_connections.erase(conn->fd());
        },
        new ThreadParam{conn, this});
  } catch (...) {
  }
}

void E::wait() noexcept {
  m_thread_pool.init();
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
      auto *conn = reinterpret_cast<Connection *>(events[i].data.ptr);
      if ((events[i].events & (EPOLLHUP | EPOLLERR)) != 0U) {
        auto *conn = reinterpret_cast<Connection *>(events[i].data.ptr);
        remove(conn);
        continue;
      }
      if ((events[i].events & EPOLLIN) != 0U) {
        if (conn == m_socket && m_is_server) {
          connect_new();
        } else {
          read_fd(conn);
        }
        continue;
      }
      if ((events[i].events & EPOLLOUT) != 0U) {
        write_fd(conn);
      }
    }
  }
}
void E::stop() noexcept {
  m_stop_f = 1;
  eventfd_write(m_event->fd(), 1);
  m_event->wait_not_using();
  m_socket->wait_not_using();
  for (auto &conn : m_connections) {
    conn.second->wait_not_using();
  }
}

void E::connect_new() noexcept {
  try {
    m_thread_pool.add(
        m_socket->fd(),
        [](void *arg) {
          auto *ptr = static_cast<ThreadParam *>(arg);
          auto *_this = ptr->epoll;
          auto *conn = ptr->conn;
          struct sockaddr_in c_addr {};
          socklen_t c_addr_len = sizeof(c_addr);
          auto *ca_cast = reinterpret_cast<struct sockaddr *>(&c_addr);
          int c_fd = accept(conn->fd(), ca_cast, &c_addr_len);
          if (c_fd == -1) {
            _this->modify(_this->m_socket);
            return;
          }
          try {
            auto *nconn = _this->add(c_fd);
            _this->m_connections[c_fd] = nconn;

            if (_this->m_connected_cb != nullptr) {
              _this->m_connected_cb(*nconn, _this->m_connected_arg);
            }
          } catch (const std::runtime_error &e) {
          }
          _this->modify(_this->m_socket);
          delete ptr;
        },
        new ThreadParam{m_socket, this});
  } catch (...) {
  }
}
void E::read_fd(Connection *conn) noexcept {
  try {
    m_thread_pool.add(
        conn->fd(),
        [](void *arg) {
          auto *ptr = static_cast<ThreadParam *>(arg);
          auto *_this = ptr->epoll;
          auto *conn = ptr->conn;
          char buffer[1024];
          char *buffer_ptr = buffer;
          ssize_t bytes_read = sizeof(buffer);
          conn->read(buffer_ptr, bytes_read);
          if (bytes_read < 1) {
            if (errno != EAGAIN) {
              _this->remove(conn);
              return;
            }
            _this->modify(conn);
            return;
          }
          if (_this->m_readed_cb != nullptr) {
            _this->m_readed_cb(conn->fd(), buffer, bytes_read, _this->m_readed_arg);
          }
          _this->modify(conn);
          delete ptr;
        },
        new ThreadParam{conn, this});
  } catch (...) {
  }
}
void E::write_fd(Connection *conn) noexcept {
  try {
    m_thread_pool.add(
        conn->fd(),
        [](void *arg) {
          auto *ptr = static_cast<ThreadParam *>(arg);
          auto *_this = ptr->epoll;
          auto *conn = ptr->conn;
          if (!conn->write()) {
            _this->remove(conn);
            return;
          }
          if (_this->m_wrote_cb != nullptr) {
            _this->m_wrote_cb(conn->fd(), _this->m_wrote_arg);
          }
          _this->modify(conn);
          delete ptr;
        },
        new ThreadParam{conn, this});
  } catch (...) {
  }
}
