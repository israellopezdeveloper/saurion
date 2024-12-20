#include "client_interface.hpp" // for ClientInterface
#include "low_saurion.h"        // for saurion

#include <fcntl.h>    // for open, O_WRONLY
#include <stdio.h>    // for perror, printf
#include <stdlib.h>   // for malloc
#include <string.h>   // for strcmp
#include <sys/stat.h> // for mkfifo
#include <unistd.h>   // for write

// cls; export LD_LIBRARY_PATH=build/lib/.libs/:$LD_LIBRARY_PATH; valgrind
// --leak-check=full --show-leak-kinds=all --track-origins=yes -s
// ./build/tests/.libs/saurion_leak -p holatiomierdas

#define PORT 8080

static pthread_mutex_t connected_m;
static pthread_cond_t connected_c;
static uint connected = 0;
static int fds[50];
static pthread_mutex_t readed_m;
static pthread_cond_t readed_c;
static uint readed = 0;
static pthread_mutex_t wrote_m;
static pthread_cond_t wrote_c;
static uint wrote = 0;
static ClientInterface client;

// Callbacks
//    -> OnConnected
static void
cb_OnConnected (int sfd, void *)
{
  pthread_mutex_lock (&connected_m);
  for (int i = 0; i < 50; ++i)
    {
      if (fds[i] < 0)
        {
          fds[i] = sfd;
          ++connected;
          break;
        }
    }
  pthread_cond_signal (&connected_c);
  pthread_mutex_unlock (&connected_m);
}
//    -> OnReaded
static void
cb_OnReaded (int sfd, const void *const msg, const int64_t size, void *)
{
  char *res = (char *)malloc ((uint64_t)size + 1);
  memcpy (res, msg, size + 1);
  res[size] = '\0';
  printf ("read %d -> %s (%ld)\n", sfd, res, size);
  free (res);
  pthread_mutex_lock (&readed_m);
  readed += size;
  pthread_cond_signal (&readed_c);
  pthread_mutex_unlock (&readed_m);
}
//    -> OnWrote
static void
cb_OnWrote (int sfd, void *)
{
  printf ("write -> (%d)\n", sfd);
  pthread_mutex_lock (&wrote_m);
  ++wrote;
  pthread_cond_signal (&wrote_c);
  pthread_mutex_unlock (&wrote_m);
}
//    -> OnClosed
static void
cb_OnClosed (int sfd, void *)
{
  printf ("close -> %d\n", sfd);
  pthread_mutex_lock (&connected_m);
  for (int i = 0; i < 50; ++i)
    {
      if (fds[i] == sfd)
        {
          fds[i] = -1;
          --connected;
          break;
        }
    }
  pthread_cond_signal (&connected_c);
  pthread_mutex_unlock (&connected_m);
}
//    -> OnError
static void
cb_OnError (int, const char *const, const int64_t, void *)
{
  // Not tested yet
}

void
create_saurion (struct saurion **s)
{
  pthread_mutex_init (&connected_m, NULL);
  pthread_cond_init (&connected_c, NULL);
  connected = 0;
  for (int i = 0; i < 50; ++i)
    {
      fds[i] = -1;
    }
  pthread_mutex_init (&readed_m, NULL);
  pthread_cond_init (&readed_c, NULL);
  readed = 0;
  *s = saurion_create (4);
  (*s)->ss = saurion_set_socket (client.getPort ());
  if (!(*s)->ss)
    {
      perror ("socket");
    }
  (*s)->cb.on_connected = cb_OnConnected;
  (*s)->cb.on_connected_arg = NULL;
  (*s)->cb.on_readed = cb_OnReaded;
  (*s)->cb.on_readed_arg = NULL;
  (*s)->cb.on_wrote = cb_OnWrote;
  (*s)->cb.on_wrote_arg = NULL;
  (*s)->cb.on_closed = cb_OnClosed;
  (*s)->cb.on_closed_arg = NULL;
  (*s)->cb.on_error = cb_OnError;
  (*s)->cb.on_error_arg = NULL;
  if (!saurion_start (*s))
    {
      perror ("saurion_create");
    }
}

void
stop_saurion (struct saurion *s)
{
  saurion_stop (s);
  saurion_destroy (s);
}

void
saurion_2_clients (struct saurion *s, int n, const char *msg)
{
  pthread_mutex_lock (&connected_m);
  for (int i = 0; i < 50; ++i)
    {
      for (int j = 0; j < n; ++j)
        {
          saurion_send (s, fds[i], msg);
        }
    }
  pthread_mutex_unlock (&connected_m);
}

void
wait_connected (const uint n)
{
  pthread_mutex_lock (&connected_m);
  while (connected != n)
    {
      pthread_cond_wait (&connected_c, &connected_m);
    }
  pthread_mutex_unlock (&connected_m);
}

void
wait_readed (const uint n)
{
  pthread_mutex_lock (&readed_m);
  while (readed != n)
    {
      pthread_cond_wait (&readed_c, &readed_m);
    }
  pthread_mutex_unlock (&readed_m);
}

void
wait_wrote (const uint n)
{
  pthread_mutex_lock (&wrote_m);
  while (wrote != n)
    {
      pthread_cond_wait (&wrote_c, &wrote_m);
    }
  pthread_mutex_unlock (&wrote_m);
}

int
main ()
{
  const uint clients = 4;
  const uint msgs = 20;
  const char *const msg = "Hola tio mierdas";

  struct saurion *s = NULL;
  create_saurion (&s);

  client.connect (clients);
  wait_connected (clients);

  client.send (msgs, msg, 0);
  wait_readed (clients * msgs * strlen (msg));

  saurion_2_clients (s, msgs, msg);
  wait_wrote (clients * msgs);

  stop_saurion (s);
  return 0;
}
