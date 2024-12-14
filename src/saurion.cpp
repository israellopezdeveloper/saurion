#include "saurion.hpp"
#include "low_saurion.h" // for saurion, saurion_create, saurion_destroy

#include <stdexcept> // for runtime_error
#include <unistd.h>  // close

Saurion::Saurion (const uint32_t thds, const int sck) noexcept
{
  this->s = saurion_create (thds);
  if (!this->s)
    {
      return;
    }
  this->s->ss = sck;
}

Saurion::~Saurion ()
{
  close (s->ss);
  saurion_destroy (this->s);
}

void
Saurion::init ()
{
  if (!saurion_start (this->s))
    {
      throw std::runtime_error ("Error on saurion start");
    }
}

void
Saurion::stop () const noexcept
{
  saurion_stop (this->s);
}

Saurion *
Saurion::on_connected (Saurion::ConnectedCb ncb, void *arg) noexcept
{
  s->cb.on_connected = ncb;
  s->cb.on_connected_arg = arg;
  return this;
}

Saurion *
Saurion::on_readed (Saurion::ReadedCb ncb, void *arg) noexcept
{
  s->cb.on_readed = ncb;
  s->cb.on_readed_arg = arg;
  return this;
}

Saurion *
Saurion::on_wrote (Saurion::WroteCb ncb, void *arg) noexcept
{
  s->cb.on_wrote = ncb;
  s->cb.on_wrote_arg = arg;
  return this;
}

Saurion *
Saurion::on_closed (Saurion::ClosedCb ncb, void *arg) noexcept
{
  s->cb.on_closed = ncb;
  s->cb.on_closed_arg = arg;
  return this;
}

Saurion *
Saurion::on_error (Saurion::ErrorCb ncb, void *arg) noexcept
{
  s->cb.on_error = ncb;
  s->cb.on_error_arg = arg;
  return this;
}

void
Saurion::send (const int fd, const char *const msg) noexcept
{
  saurion_send (this->s, fd, msg);
}
