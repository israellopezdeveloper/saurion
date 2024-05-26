// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "connection.hpp"

#include <unistd.h>

#include <cstdio>
#include <cstring>

using C = Connection;

C::Connection(const int nfd) noexcept
    : m_fd(nfd), m_previous(nullptr), m_previous_len(0), m_queue_mtx(PTHREAD_MUTEX_INITIALIZER) {}
C::~Connection() noexcept {
  delete[] m_previous;
  pthread_mutex_lock(&m_queue_mtx);
  while (!m_messages.empty()) {
    m_messages.pop();
  }
  pthread_mutex_unlock(&m_queue_mtx);
  close();
}

void C::read(char *&buffer, ssize_t &len) {
  fprintf(stderr, "==>==>==>==>==><%d> read something\n", m_fd);
  if ((m_previous != nullptr) && (m_previous_len > 0)) {
    memcpy(buffer, m_previous, m_previous_len);
  }
  ssize_t nbytes = ::read(m_fd, buffer + m_previous_len, len - m_previous_len);
  fprintf(stderr, "==>==>==>==>==><%d> read %ld bytes\n", m_fd, nbytes);
  if (nbytes == -1) {
    len = -1;
    return;
  }
  if (nbytes == 0) {
    len = 0;
    return;
  }
  size_t total = m_previous_len + nbytes;
  while ((total > 0) && (buffer[total - 1] != '\0')) {
    total--;
  }
  if (total < m_previous_len + nbytes) {
    size_t rest = (m_previous_len + nbytes) - total;
    delete[] m_previous;
    m_previous = nullptr;
    m_previous_len = 0;
    m_previous = new char[rest];
    memcpy(m_previous, buffer + total, rest);
    m_previous_len = rest;
  }
  len = static_cast<ssize_t>(total);
}
bool C::write() noexcept {
  while (true) {
    pthread_mutex_lock(&m_queue_mtx);
    if (m_messages.empty()) {
      pthread_mutex_unlock(&m_queue_mtx);
      break;
    }
    auto &message = m_messages.front();
    pthread_mutex_unlock(&m_queue_mtx);
    ssize_t nbytes = 0;
    size_t written = 0;
    while (nbytes < message->second) {
      nbytes = ::write(m_fd, message->first + written, message->second - written);
      if (nbytes == -1) {
        return false;
      }
      written += nbytes;
    }
    delete[] message->first;
    delete message;
    pthread_mutex_lock(&m_queue_mtx);
    m_messages.pop();
    pthread_mutex_unlock(&m_queue_mtx);
  }
  return true;
}
void C::send(const char *data, const size_t len) noexcept {
  pthread_mutex_lock(&m_queue_mtx);
  try {
    char *buffer = new char[len + 1];
    memcpy(buffer, data, len);
    buffer[len] = '\0';
    auto *item = new std::pair<char *, const ssize_t>(buffer, len + 1);
    m_messages.push(item);
  } catch (...) {
  }
  pthread_mutex_unlock(&m_queue_mtx);
}
void C::close() const noexcept { ::close(m_fd); }

int C::fd() const noexcept { return m_fd; }
