#ifndef SAURION_HPP
#define SAURION_HPP

#include <liburing.h>

#include <cstddef>
#include <map>

#include "low_saurion.h"
#include "threadpool.hpp"

#define QUEUE_DEPTH 256

class Saurion {
 public:
  using ConnectedCb = void (*)(const int, void *);
  using ReadedCb = void (*)(const int, const char *const, const ssize_t, void *);
  using WroteCb = void (*)(const int, void *);
  using ClosedCb = void (*)(const int, void *);
  using ErrorCb = void (*)(const int, const char *const, const ssize_t, void *);

  explicit Saurion(const int svr, const int thds) noexcept;
  ~Saurion();

  Saurion(const Saurion &) = delete;
  Saurion(Saurion &&) = delete;
  Saurion &operator=(const Saurion &) = delete;
  Saurion &operator=(Saurion &&) = delete;

  void init() noexcept;
  void stop() noexcept;

  void on_connected(ConnectedCb ncb, void *arg) noexcept;
  void on_readed(ReadedCb ncb, void *arg) noexcept;
  void on_wrote(WroteCb ncb, void *arg) noexcept;
  void on_closed(ClosedCb ncb, void *arg) noexcept;
  void on_error(ErrorCb ncb, void *arg) noexcept;

 private:
  const int m_server;
  const int m_efd;
  struct io_uring m_ring;
  bool m_stop;
  ThreadPool m_pool;
  std::map<size_t, struct request *> m_requests;

  ConnectedCb m_connected_cb = nullptr;
  void *m_connected_arg = nullptr;
  ReadedCb m_readed_cb = nullptr;
  void *m_readed_arg = nullptr;
  WroteCb m_wrote_cb = nullptr;
  void *m_wrote_arg = nullptr;
  ClosedCb m_closed_cb = nullptr;
  void *m_closed_arg = nullptr;
  ErrorCb m_error_cb = nullptr;
  void *m_error_arg = nullptr;

  static void init_thread(void *arg) noexcept;

  void handle_accept(struct request *req, struct io_uring_cqe *cqe, struct result *res) noexcept;
  void handle_read(struct request *req, struct io_uring_cqe *cqe, struct result *res) noexcept;
  void handle_error(const int nfd, const char *const msg) const noexcept;

  size_t accept_n_store() noexcept;

  size_t read_n_store(const int client) noexcept;

  void cleanup(struct request *req, struct io_uring_cqe *cqe, struct result *res) noexcept;
};

#endif  // !SAURION_HPP
