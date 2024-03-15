// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "connection.hpp"

#include <unistd.h>

#include <cstdio>
#include <cstring>

#define NOTHING 0x00
#define READ 0x01
#define WRITE 0x02

using C = Connection;

C::Connection(const int fd) noexcept
    : m_fd(fd),
      m_availability(READ | WRITE),
      m_status(NOTHING),
      m_queue_mtx(PTHREAD_MUTEX_INITIALIZER) {}
C::~Connection() noexcept {
  pthread_mutex_lock(&m_queue_mtx);
  while (!m_messages.empty()) {
    m_messages.pop();
  }
  pthread_mutex_unlock(&m_queue_mtx);
  close();
}

void C::read(char *&buffer, ssize_t &len) noexcept {
  readable(false);
  reading(true);
  ssize_t n = ::read(m_fd, buffer, len);
  if (n == -1) {
    len = -1;
    reading(false);
    readable(false);
    return;
  }
  if (n == 0) {
    len = 0;
    reading(false);
    readable(true);
    return;
  }
  len = n;
  reading(false);
  readable(true);
}
bool C::write() noexcept {
  writeable(true);
  ssize_t total_written = 0;
  while (true) {
    pthread_mutex_lock(&m_queue_mtx);
    if (m_messages.empty()) {
      pthread_mutex_unlock(&m_queue_mtx);
      break;
    }
    auto message = m_messages.front();
    pthread_mutex_unlock(&m_queue_mtx);
    ssize_t n = ::write(m_fd, message->first, message->second);
    if (n == -1) {
      writing(false);
      writeable(true);
      return false;
    }
    total_written += n;
    delete[] const_cast<char *>(message->first);
    delete message;
    pthread_mutex_lock(&m_queue_mtx);
    m_messages.pop();
    pthread_mutex_unlock(&m_queue_mtx);
  }
  writing(false);
  writeable(true);
  return true;
}
void C::send(const char *data, const size_t len) noexcept {
  pthread_mutex_lock(&m_queue_mtx);
  char *buffer = new char[len + 1];
  memcpy(buffer, data, len);
  buffer[len] = '\0';
  auto *item = new std::pair<const char *, const ssize_t>(buffer, len + 1);
  m_messages.push(item);
  pthread_mutex_unlock(&m_queue_mtx);
}
void C::close() noexcept { ::close(m_fd); }

int C::fd() const noexcept { return m_fd; }
bool C::is_readable() const noexcept { return (m_availability & READ) != 0; }
bool C::is_writable() const noexcept { return (m_availability & WRITE) != 0; }

void C::readable(bool val) noexcept {
  m_availability = (m_availability & ~READ) | (val ? READ : NOTHING);
}
void C::writeable(bool val) noexcept {
  m_availability = (m_availability & ~WRITE) | (val ? WRITE : NOTHING);
}

bool C::is_reading() const noexcept { return (m_status & READ) != 0; }
bool C::is_writing() const noexcept { return (m_status & WRITE) != 0; }
void C::reading(bool val) noexcept { m_status = (m_status & ~READ) | (val ? READ : NOTHING); }
void C::writing(bool val) noexcept { m_status = (m_status & ~WRITE) | (val ? WRITE : NOTHING); }
