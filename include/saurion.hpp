#ifndef SAURION_HPP
#define SAURION_HPP

#include <stdint.h>    // for uint32_t
#include <sys/types.h> // for ssize_t

class Saurion
{
public:
  using ConnectedCb = void (*) (const int, void *);
  using ReadedCb
      = void (*) (const int, const void *const, const ssize_t, void *);
  using WroteCb = void (*) (const int, void *);
  using ClosedCb = void (*) (const int, void *);
  using ErrorCb
      = void (*) (const int, const char *const, const ssize_t, void *);

  explicit Saurion (const uint32_t thds, const int sck) noexcept;
  ~Saurion ();

  Saurion (const Saurion &) = delete;
  Saurion (Saurion &&) = delete;
  Saurion &operator= (const Saurion &) = delete;
  Saurion &operator= (Saurion &&) = delete;

  void init () noexcept;
  void stop () noexcept;

  Saurion *on_connected (ConnectedCb ncb, void *arg) noexcept;
  Saurion *on_readed (ReadedCb ncb, void *arg) noexcept;
  Saurion *on_wrote (WroteCb ncb, void *arg) noexcept;
  Saurion *on_closed (ClosedCb ncb, void *arg) noexcept;
  Saurion *on_error (ErrorCb ncb, void *arg) noexcept;

  void send (const int fd, const char *const msg) noexcept;

private:
  struct saurion *s;
};

#endif // !SAURION_HPP
