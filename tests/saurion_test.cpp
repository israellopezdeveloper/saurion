#include "client_interface.hpp"
#include "config.h"
#include "low_saurion.h"
#include "saurion.hpp"

#include <cstdio>
#include <ctime>
#include <memory>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "gtest/gtest.h"

struct summary
{
  explicit summary () = default;
  ~summary () = default;
  summary (const summary &) = delete;
  summary (summary &&) = delete;
  summary &operator= (const summary &) = delete;
  summary &operator= (summary &&) = delete;
  uint32_t connected = 0;
  std::vector<int> fds;
  pthread_cond_t connected_c = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t connected_m = PTHREAD_MUTEX_INITIALIZER;
  uint32_t disconnected = 0;
  pthread_cond_t disconnected_c = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t disconnected_m = PTHREAD_MUTEX_INITIALIZER;
  size_t readed = 0;
  pthread_cond_t readed_c = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t readed_m = PTHREAD_MUTEX_INITIALIZER;
  uint32_t wrote = 0;
  pthread_cond_t wrote_c = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t wrote_m = PTHREAD_MUTEX_INITIALIZER;
} __attribute__ ((aligned (128)));

// Callbacks
//    -> OnConnected
static void
cb_OnConnected (int sfd, void *arg)
{
  auto *summary = static_cast<struct summary *> (arg);
  pthread_mutex_lock (&summary->connected_m);
  summary->connected++;
  summary->fds.push_back (sfd);
  pthread_cond_signal (&summary->connected_c);
  pthread_mutex_unlock (&summary->connected_m);
}
//    -> OnReaded
static void
cb_OnReaded (int, const void *const, const ssize_t size, void *arg)
{
  auto *summary = static_cast<struct summary *> (arg);
  pthread_mutex_lock (&summary->readed_m);
  summary->readed += size;
  pthread_cond_signal (&summary->readed_c);
  pthread_mutex_unlock (&summary->readed_m);
}
//    -> OnWrote
static void
cb_OnWrote (int, void *arg)
{
  auto *summary = static_cast<struct summary *> (arg);
  pthread_mutex_lock (&summary->wrote_m);
  summary->wrote++;
  pthread_cond_signal (&summary->wrote_c);
  pthread_mutex_unlock (&summary->wrote_m);
}
//    -> OnClosed
static void
cb_OnClosed (int sfd, void *arg)
{
  auto *summary = static_cast<struct summary *> (arg);
  pthread_mutex_lock (&summary->disconnected_m);
  atomic_fetch_add_explicit ((atomic_int *)&summary->disconnected, 1,
                             memory_order_relaxed);
  pthread_cond_signal (&summary->disconnected_c);
  pthread_mutex_unlock (&summary->disconnected_m);
  pthread_mutex_lock (&summary->connected_m);
  auto &vec = summary->fds;
  vec.erase (std::remove (vec.begin (), vec.end (), sfd), vec.end ());
  pthread_cond_signal (&summary->connected_c);
  pthread_mutex_unlock (&summary->connected_m);
}
//    -> OnError
static void
cb_OnError (int, const char *const, const ssize_t, void *)
{
  // Not tested yet
}

class CommonSaurion
{
public:
  struct summary summary;

  // SetUp
  void
  SetUp ()
  {
    summary.connected = 0;
    summary.disconnected = 0;
    summary.readed = 0;
    summary.wrote = 0;
    summary.fds.clear ();
  }

  // TearDown
  void
  TearDown ()
  {
    struct timespec tim;
    tim.tv_sec = 0;
    tim.tv_nsec = 15000000L;
    nanosleep (&tim, nullptr);
  }

  // wait_connected
  void
  wait_connected (const uint n)
  {
    pthread_mutex_lock (&summary.connected_m);
    while (summary.connected != n)
      {
        pthread_cond_wait (&summary.connected_c, &summary.connected_m);
      }
    pthread_mutex_unlock (&summary.connected_m);
  }

  // wait_disconnected
  void
  wait_disconnected (uint32_t n)
  {
    pthread_mutex_lock (&summary.disconnected_m);
    while (summary.disconnected != n)
      {
        pthread_cond_wait (&summary.disconnected_c, &summary.disconnected_m);
      }
    pthread_mutex_unlock (&summary.disconnected_m);
  }

  // wait_read
  void
  wait_readed (size_t n)
  {
    pthread_mutex_lock (&summary.readed_m);
    while (summary.readed < n)
      {
        pthread_cond_wait (&summary.readed_c, &summary.readed_m);
      }
    pthread_mutex_unlock (&summary.readed_m);
  }

  // wait_wrote
  void
  wait_wrote (uint32_t n)
  {
    pthread_mutex_lock (&summary.wrote_m);
    while (summary.wrote < n)
      {
        pthread_cond_wait (&summary.wrote_c, &summary.wrote_m);
      }
    pthread_mutex_unlock (&summary.wrote_m);
  }
};

class LowSaurion : public CommonSaurion
{
private:
  struct saurion *saurion;

public:
  // SetUp
  void
  SetUp (const uint port)
  {
    CommonSaurion::SetUp ();
    const unsigned int N_THREADS = 6;
    saurion = saurion_create (N_THREADS);
    if (!saurion)
      {
        return;
      }
    saurion->ss = saurion_set_socket (port);
    if (!saurion->ss)
      {
        throw std::runtime_error (strerror (errno));
      }
    saurion->cb.on_connected = cb_OnConnected;
    saurion->cb.on_connected_arg = &summary;
    saurion->cb.on_readed = cb_OnReaded;
    saurion->cb.on_readed_arg = &summary;
    saurion->cb.on_wrote = cb_OnWrote;
    saurion->cb.on_wrote_arg = &summary;
    saurion->cb.on_closed = cb_OnClosed;
    saurion->cb.on_closed_arg = &summary;
    saurion->cb.on_error = cb_OnError;
    saurion->cb.on_error_arg = &summary;
    if (!saurion_start (saurion))
      {
        exit (ERROR_CODE);
      }
  }

  // TearDown
  void
  TearDown ()
  {
    saurion_stop (saurion);
    close (saurion->ss);
    saurion_destroy (saurion);
    CommonSaurion::TearDown ();
  }

  // send
  void
  send (const int sfd, const uint32_t n, const char *const msg)
  {
    for (uint32_t i = 0; i < n; ++i)
      {
        saurion_send (saurion, sfd, msg);
      }
  }

  // sendAll
  void
  sendAll (const uint32_t n, const char *const msg)
  {
    for (auto sfd : summary.fds)
      {
        for (uint32_t i = 0; i < n; ++i)
          {
            saurion_send (saurion, sfd, msg);
          }
      }
  }
};

class HighSaurion : public CommonSaurion
{
private:
  Saurion *saurion;

public:
  // SetUp
  void
  SetUp (const uint port)
  {
    CommonSaurion::SetUp ();
    const unsigned int N_THREADS = 6;
    saurion = new Saurion (N_THREADS, saurion_set_socket (port));
    saurion->on_connected (cb_OnConnected, &summary)
        ->on_readed (cb_OnReaded, &summary)
        ->on_wrote (cb_OnWrote, &summary)
        ->on_closed (cb_OnClosed, &summary)
        ->on_error (cb_OnError, &summary);
    saurion->init ();
  }

  // TearDown
  void
  TearDown ()
  {
    CommonSaurion::TearDown ();
    saurion->stop ();
    delete saurion;
  }

  // send
  void
  send (const int sfd, const uint32_t n, const char *const msg)
  {
    for (uint32_t i = 0; i < n; ++i)
      {
        saurion->send (sfd, msg);
      }
  }

  // sendAll
  void
  sendAll (const uint32_t n, const char *const msg)
  {
    for (auto sfd : summary.fds)
      {
        for (uint32_t i = 0; i < n; ++i)
          {
            saurion->send (sfd, msg);
          }
      }
  }
};

template <typename SaurionType> class SaurionTest : public ::testing::Test
{
public:
  SaurionType saurion;
  ClientInterface client;

protected:
  void
  SetUp () override
  {
    saurion.SetUp (client.getPort ());
  }

  void
  TearDown () override
  {
    client.disconnect ();
    client.clean ();
    saurion.TearDown ();
  }
};

typedef ::testing::Types<LowSaurion, HighSaurion> SaurionTypes;
TYPED_TEST_SUITE (SaurionTest, SaurionTypes);

TYPED_TEST (SaurionTest, initServerAndCloseCorrectly) { EXPECT_TRUE (true); }

TYPED_TEST (SaurionTest, connectMultipleClients)
{
  uint32_t clients = 20;
  this->client.connect (clients);
  this->saurion.wait_connected (clients);
  EXPECT_EQ (this->saurion.summary.connected, clients);
  this->client.disconnect ();
  this->saurion.wait_disconnected (clients);
  EXPECT_EQ (this->saurion.summary.disconnected, clients);
}

TYPED_TEST (SaurionTest, readWriteMsgsToClients)
{
  uint32_t clients = 20;
  uint32_t msgs = 100;
  this->client.connect (clients);
  this->saurion.wait_connected (clients);
  EXPECT_EQ (this->saurion.summary.connected, clients);
  this->saurion.sendAll (msgs, "Hola");
  this->client.send (msgs, "Hola", 0);
  this->saurion.wait_readed (msgs * clients * 4);
  EXPECT_EQ (this->saurion.summary.readed, msgs * clients * 4);
  this->client.disconnect ();
  this->saurion.wait_disconnected (clients);
  EXPECT_EQ (this->saurion.summary.disconnected, clients);
}

TYPED_TEST (SaurionTest, reconnectClients)
{
  uint32_t clients = 5;
  this->client.connect (clients);
  this->saurion.wait_connected (clients);
  EXPECT_EQ (this->saurion.summary.connected, clients);
  this->client.disconnect ();
  this->saurion.wait_disconnected (clients);
  EXPECT_EQ (this->saurion.summary.disconnected, clients);
  this->client.connect (clients);
  this->saurion.wait_connected (clients * 2);
  EXPECT_EQ (this->saurion.summary.connected, clients * 2);
  this->client.disconnect ();
  this->saurion.wait_disconnected (clients * 2);
  EXPECT_EQ (this->saurion.summary.disconnected, clients * 2);
}

TYPED_TEST (SaurionTest, readWriteWithLargeMessageMultipleOfChunkSize)
{
  uint32_t clients = 1;
  size_t size = CHUNK_SZ * 2;
  auto str = std::make_unique<char[]> (size + 1);
  std::memset (str.get (), 'A', size);
  str[size - 1] = '1';
  str[size] = 0;
  this->client.connect (clients);
  this->saurion.wait_connected (clients);
  EXPECT_EQ (this->saurion.summary.connected, clients);
  this->client.send (1, str.get (), 0);
  this->saurion.wait_readed (size);
  EXPECT_EQ (this->saurion.summary.readed, size);
  this->saurion.send (this->saurion.summary.fds.front (), 1, str.get ());
  this->saurion.wait_wrote (1);
  this->client.disconnect ();
  this->saurion.wait_disconnected (clients);
  EXPECT_EQ (1UL, this->client.reads (std::string (str.get ())));
  EXPECT_EQ (this->saurion.summary.disconnected, clients);
}

TYPED_TEST (SaurionTest, handleConcurrentReadsAndWrites)
{
  uint32_t clients = 20;
  uint32_t msgs = 10;
  this->client.connect (clients);
  this->saurion.wait_connected (clients);
  EXPECT_EQ (this->saurion.summary.connected, clients);
  this->client.send (msgs, "Hola", 0);
  this->saurion.sendAll (msgs, "Hola");
  this->saurion.wait_readed (msgs * clients * 4);
  EXPECT_EQ (msgs * clients * 4, this->saurion.summary.readed);
  this->saurion.wait_wrote (msgs * clients);
  EXPECT_EQ (msgs * clients, this->saurion.summary.wrote);
  this->client.disconnect ();
  this->saurion.wait_disconnected (clients);
}
