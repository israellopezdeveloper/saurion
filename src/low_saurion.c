#include "low_saurion.h"
#include "config.h"      // for ERROR_CODE, SUCCESS_CODE, CHUNK_SZ
#include "linked_list.h" // for list_delete_node, list_free, list_insert
#include "threadpool.h"  // for threadpool_add, threadpool_create

#include <arpa/inet.h>             // for htonl, ntohl, htons
#include <bits/socket-constants.h> // for SOL_SOCKET, SO_REUSEADDR
#include <liburing.h>          // for io_uring_get_sqe, io_uring, io_uring_...
#include <liburing/io_uring.h> // for io_uring_cqe
#include <nanologger.h>        // for LOG_END, LOG_INIT
#include <netinet/in.h>        // for sockaddr_in, INADDR_ANY, in_addr
#include <pthread.h>           // for pthread_mutex_lock, pthread_mutex_unlock
#include <stdint.h>            // for uint32_t, uint64_t, uint8_t
#include <stdio.h>             // for NULL
#include <stdlib.h>            // for free, malloc
#include <string.h>            // for memset, memcpy, strlen
#include <sys/eventfd.h>       // for eventfd, EFD_NONBLOCK
#include <sys/socket.h>        // for socklen_t, bind, listen, setsockopt
#include <sys/uio.h>           // for iovec
#include <time.h>              // for nanosleep
#include <unistd.h>            // for close, write

struct Node;
struct iovec;

#define EV_ACC 0 //! @brief Event type for accepting a new connection.
#define EV_REA 1 //! @brief Event type for reading data.
#define EV_WRI 2 //! @brief Event type for writing data.
#define EV_WAI 3 //! @brief Event type for waiting.
#define EV_ERR 4 //! @brief Event type to indicate an error.

struct request
{
  void *prev;
  size_t prev_size;
  size_t prev_remain;
  size_t next_iov;
  size_t next_offset;
  int event_type;
  size_t iovec_count;
  int client_socket;
  struct iovec iov[];
};

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static struct timespec TIMEOUT_RETRY_SPEC = { 0, TIMEOUT_RETRY * 1000L };

struct saurion_wrapper
{
  struct saurion *s;
  uint32_t sel;
};

static inline uint32_t
next (struct saurion *s)
{
  s->next = (s->next + 1) % s->n_threads;
  return s->next;
}

static inline uint64_t
htonll (uint64_t value)
{
  int num = 42;
  if (*(char *)&num == 42)
    {
      uint32_t high_part = htonl ((uint32_t)(value >> 32));
      uint32_t low_part = htonl ((uint32_t)(value & 0xFFFFFFFFLL));
      return ((uint64_t)low_part << 32) | high_part;
    }
  return value;
}

static inline uint64_t
ntohll (uint64_t value)
{
  int num = 42;
  if (*(char *)&num == 42)
    {
      uint32_t high_part = ntohl ((uint32_t)(value >> 32));
      uint32_t low_part = ntohl ((uint32_t)(value & 0xFFFFFFFFLL));
      return ((uint64_t)low_part << 32) | high_part;
    }
  return value;
}

void
free_request (struct request *req, void **children_ptr, size_t amount)
{
  if (children_ptr)
    {
      free (children_ptr);
      children_ptr = NULL;
    }
  for (size_t i = 0; i < amount; ++i)
    {
      free (req->iov[i].iov_base);
      req->iov[i].iov_base = NULL;
    }
  free (req);
  req = NULL;
  free (children_ptr);
  children_ptr = NULL;
}

[[nodiscard]]
int
initialize_iovec (struct iovec *iov, size_t amount, size_t pos,
                  const void *msg, size_t size, uint8_t h)
{
  if (!iov || !iov->iov_base)
    {
      return ERROR_CODE;
    }
  if (msg)
    {
      size_t len = iov->iov_len;
      char *dest = (char *)iov->iov_base;
      char *orig = (char *)msg + pos * CHUNK_SZ;
      size_t cpy_sz = 0;
      if (h)
        {
          if (pos == 0)
            {
              uint64_t send_size = htonll (size);
              memcpy (dest, &send_size, sizeof (uint64_t));
              dest += sizeof (uint64_t);
              len -= sizeof (uint64_t);
            }
          else
            {
              orig -= sizeof (uint64_t);
            }
          if ((pos + 1) == amount)
            {
              --len;
              cpy_sz = (len < size ? len : size);
              dest[cpy_sz] = 0;
            }
        }
      cpy_sz = (len < size ? len : size);
      memcpy (dest, orig, cpy_sz);
      dest += cpy_sz;
      size_t rem = CHUNK_SZ - (dest - (char *)iov->iov_base);
      memset (dest, 0, rem);
    }
  else
    {
      memset ((char *)iov->iov_base, 0, CHUNK_SZ);
    }
  return SUCCESS_CODE;
}

[[nodiscard]]
int
allocate_iovec (struct iovec *iov, size_t amount, size_t pos, size_t size,
                void **chd_ptr)
{
  if (!iov || !chd_ptr)
    {
      return ERROR_CODE;
    }
  iov->iov_base = malloc (CHUNK_SZ);
  if (!iov->iov_base)
    {
      return ERROR_CODE;
    }
  iov->iov_len = (pos == (amount - 1) ? (size % CHUNK_SZ) : CHUNK_SZ);
  if (iov->iov_len == 0)
    {
      iov->iov_len = CHUNK_SZ;
    }
  chd_ptr[pos] = iov->iov_base;
  return SUCCESS_CODE;
}

[[nodiscard]]
int
set_request (struct request **r, struct Node **l, size_t s, const void *m,
             uint8_t h)
{
  uint64_t full_size = s;
  if (h)
    {
      full_size += (sizeof (uint64_t) + sizeof (uint8_t));
    }
  size_t amount = full_size / CHUNK_SZ;
  amount = amount + (full_size % CHUNK_SZ == 0 ? 0 : 1);
  struct request *temp = (struct request *)malloc (
      sizeof (struct request) + sizeof (struct iovec) * amount);
  if (!temp)
    {
      return ERROR_CODE;
    }
  if (!*r)
    {
      *r = temp;
      (*r)->prev = NULL;
      (*r)->prev_size = 0;
      (*r)->prev_remain = 0;
      (*r)->next_iov = 0;
      (*r)->next_offset = 0;
    }
  else
    {
      temp->client_socket = (*r)->client_socket;
      temp->event_type = (*r)->event_type;
      temp->prev = (*r)->prev;
      temp->prev_size = (*r)->prev_size;
      temp->prev_remain = (*r)->prev_remain;
      temp->next_iov = (*r)->next_iov;
      temp->next_offset = (*r)->next_offset;
      *r = temp;
    }
  struct request *req = *r;
  req->iovec_count = (int)amount;
  void **children_ptr = (void **)malloc (amount * sizeof (void *));
  if (!children_ptr)
    {
      free_request (req, children_ptr, 0);
      return ERROR_CODE;
    }
  for (size_t i = 0; i < amount; ++i)
    {
      if (!allocate_iovec (&req->iov[i], amount, i, full_size, children_ptr))
        {
          free_request (req, children_ptr, amount);
          return ERROR_CODE;
        }
      if (!initialize_iovec (&req->iov[i], amount, i, m, s, h))
        {
          free_request (req, children_ptr, amount);
          return ERROR_CODE;
        }
    }
  if (list_insert (l, req, amount, children_ptr))
    {
      free_request (req, children_ptr, amount);
      return ERROR_CODE;
    }
  free (children_ptr);
  return SUCCESS_CODE;
}

/******************* ADDERS *******************/
static inline void
add_accept (struct saurion *const s, struct sockaddr_in *const ca,
            socklen_t *const cal)
{
  int res = ERROR_CODE;
  pthread_mutex_lock (&s->m_rings[0]);
  while (res != SUCCESS_CODE)
    {
      struct io_uring_sqe *sqe = io_uring_get_sqe (&s->rings[0]);
      while (!sqe)
        {
          sqe = io_uring_get_sqe (&s->rings[0]);
          nanosleep (&TIMEOUT_RETRY_SPEC, NULL);
        }
      struct request *req = NULL;
      if (!set_request (&req, &s->list, 0, NULL, 0))
        {
          free (sqe);
          nanosleep (&TIMEOUT_RETRY_SPEC, NULL);
          res = ERROR_CODE;
          continue;
        }
      req->client_socket = 0;
      req->event_type = EV_ACC;
      io_uring_prep_accept (sqe, s->ss, (struct sockaddr *const)ca, cal, 0);
      io_uring_sqe_set_data (sqe, req);
      if (io_uring_submit (&s->rings[0]) < 0)
        {
          free (sqe);
          list_delete_node (&s->list, req);
          nanosleep (&TIMEOUT_RETRY_SPEC, NULL);
          res = ERROR_CODE;
          continue;
        }
      res = SUCCESS_CODE;
    }
  pthread_mutex_unlock (&s->m_rings[0]);
}

static inline void
add_fd (struct saurion *const s, int client_socket, int sel)
{
  int res = ERROR_CODE;
  pthread_mutex_lock (&s->m_rings[sel]);
  while (res != SUCCESS_CODE)
    {
      struct io_uring *ring = &s->rings[sel];
      struct io_uring_sqe *sqe = io_uring_get_sqe (ring);
      while (!sqe)
        {
          sqe = io_uring_get_sqe (ring);
          nanosleep (&TIMEOUT_RETRY_SPEC, NULL);
        }
      struct request *req = NULL;
      if (!set_request (&req, &s->list, CHUNK_SZ, NULL, 0))
        {
          free (sqe);
          res = ERROR_CODE;
          continue;
        }
      req->event_type = EV_REA;
      req->client_socket = client_socket;
      io_uring_prep_readv (sqe, client_socket, &req->iov[0], req->iovec_count,
                           0);
      io_uring_sqe_set_data (sqe, req);
      if (io_uring_submit (ring) < 0)
        {
          free (sqe);
          list_delete_node (&s->list, req);
          res = ERROR_CODE;
          continue;
        }
      res = SUCCESS_CODE;
    }
  pthread_mutex_unlock (&s->m_rings[sel]);
}

static inline void
add_efd (struct saurion *const s, const int client_socket, int sel)
{
  add_fd (s, client_socket, sel);
}

static inline void
add_read (struct saurion *const s, const int client_socket)
{
  int sel = next (s);
  add_fd (s, client_socket, sel);
}

static inline void
add_read_continue (struct saurion *const s, struct request *oreq,
                   const int sel)
{
  pthread_mutex_lock (&s->m_rings[sel]);
  int res = ERROR_CODE;
  while (res != SUCCESS_CODE)
    {
      struct io_uring *ring = &s->rings[sel];
      struct io_uring_sqe *sqe = io_uring_get_sqe (ring);
      while (!sqe)
        {
          sqe = io_uring_get_sqe (ring);
          nanosleep (&TIMEOUT_RETRY_SPEC, NULL);
        }
      if (!set_request (&oreq, &s->list, oreq->prev_remain, NULL, 0))
        {
          free (sqe);
          res = ERROR_CODE;
          continue;
        }
      io_uring_prep_readv (sqe, oreq->client_socket, &oreq->iov[0],
                           oreq->iovec_count, 0);
      io_uring_sqe_set_data (sqe, oreq);
      if (io_uring_submit (ring) < 0)
        {
          free (sqe);
          list_delete_node (&s->list, oreq);
          res = ERROR_CODE;
          continue;
        }
      res = SUCCESS_CODE;
    }
  pthread_mutex_unlock (&s->m_rings[sel]);
}

static inline void
add_write (struct saurion *const s, int fd, const char *const str,
           const int sel)
{
  int res = ERROR_CODE;
  pthread_mutex_lock (&s->m_rings[sel]);
  while (res != SUCCESS_CODE)
    {
      struct io_uring *ring = &s->rings[sel];
      struct io_uring_sqe *sqe = io_uring_get_sqe (ring);
      while (!sqe)
        {
          sqe = io_uring_get_sqe (ring);
          nanosleep (&TIMEOUT_RETRY_SPEC, NULL);
        }
      struct request *req = NULL;
      if (!set_request (&req, &s->list, strlen (str), (const void *const)str,
                        1))
        {
          free (sqe);
          res = ERROR_CODE;
          continue;
        }
      req->event_type = EV_WRI;
      req->client_socket = fd;
      io_uring_prep_writev (sqe, req->client_socket, req->iov,
                            req->iovec_count, 0);
      io_uring_sqe_set_data (sqe, req);
      if (io_uring_submit (ring) < 0)
        {
          free (sqe);
          list_delete_node (&s->list, req);
          res = ERROR_CODE;
          nanosleep (&TIMEOUT_RETRY_SPEC, NULL);
          continue;
        }
      res = SUCCESS_CODE;
    }
  pthread_mutex_unlock (&s->m_rings[sel]);
}

/******************* HANDLERS *******************/
static inline void
handle_accept (const struct saurion *const s, const int fd)
{
  if (s->cb.on_connected)
    {
      s->cb.on_connected (fd, s->cb.on_connected_arg);
    }
}

[[nodiscard]]
int
read_chunk (void **dest, size_t *len, struct request *const req)
{
  if (req->iovec_count == 0)
    {
      return ERROR_CODE;
    }

  size_t max_iov_cont = 0; //< Total size of request
  for (size_t i = 0; i < req->iovec_count; ++i)
    {
      max_iov_cont += req->iov[i].iov_len;
    }
  size_t cont_sz = 0;
  size_t cont_rem = 0;
  size_t curr_iov = 0;
  size_t curr_iov_off = 0;
  size_t dest_off = 0;
  void *dest_ptr = NULL;
  if (req->prev && req->prev_size && req->prev_remain)
    {
      cont_sz = req->prev_size;
      cont_rem = req->prev_remain;
      curr_iov = 0;
      curr_iov_off = 0;
      dest_off = cont_sz - cont_rem;
      if (cont_rem <= max_iov_cont)
        {
          *dest = req->prev;
          dest_ptr = *dest;
          req->prev = NULL;
          req->prev_size = 0;
          req->prev_remain = 0;
        }
      else
        {
          dest_ptr = req->prev;
          *dest = NULL;
        }
    }
  else if (req->next_iov || req->next_offset)
    {
      curr_iov = req->next_iov;
      curr_iov_off = req->next_offset;
      cont_sz = *(
          (size_t *)(((uint8_t *)req->iov[curr_iov].iov_base) + curr_iov_off));
      cont_sz = ntohll (cont_sz);
      curr_iov_off += sizeof (uint64_t);
      cont_rem = cont_sz;
      dest_off = cont_sz - cont_rem;
      if ((curr_iov_off + cont_rem + 1) <= max_iov_cont)
        {
          *dest = malloc (cont_sz);
          dest_ptr = *dest;
        }
      else
        {
          req->prev = malloc (cont_sz);
          dest_ptr = req->prev;
          *dest = NULL;
          *len = 0;
        }
    }
  else
    {
      curr_iov = 0;
      curr_iov_off = 0;
      cont_sz = *(
          (size_t *)(((uint8_t *)req->iov[curr_iov].iov_base) + curr_iov_off));
      cont_sz = ntohll (cont_sz);
      curr_iov_off += sizeof (uint64_t);
      cont_rem = cont_sz;
      dest_off = cont_sz - cont_rem;
      if (cont_rem <= max_iov_cont)
        {
          *dest = malloc (cont_sz);
          dest_ptr = *dest;
        }
      else
        {
          req->prev = malloc (cont_sz);
          dest_ptr = req->prev;
          *dest = NULL;
        }
    }
  size_t curr_iov_msg_rem = 0;

  uint8_t ok = 1UL;
  while (1)
    {
      curr_iov_msg_rem
          = MIN (cont_rem, (req->iov[curr_iov].iov_len - curr_iov_off));
      memcpy ((uint8_t *)dest_ptr + dest_off,
              ((uint8_t *)req->iov[curr_iov].iov_base) + curr_iov_off,
              curr_iov_msg_rem);
      dest_off += curr_iov_msg_rem;
      curr_iov_off += curr_iov_msg_rem;
      cont_rem -= curr_iov_msg_rem;
      if (cont_rem <= 0)
        {
          if (*(((uint8_t *)req->iov[curr_iov].iov_base) + curr_iov_off) != 0)
            {
              ok = 0UL;
            }
          *len = cont_sz;
          ++curr_iov_off;
          break;
        }
      if (curr_iov_off >= (req->iov[curr_iov].iov_len))
        {
          ++curr_iov;
          if (curr_iov == req->iovec_count)
            {
              break;
            }
          curr_iov_off = 0;
        }
    }

  if (req->prev)
    {
      req->prev_size = cont_sz;
      req->prev_remain = cont_rem;
      *dest = NULL;
      len = 0;
    }
  else
    {
      req->prev_size = 0;
      req->prev_remain = 0;
    }
  if (curr_iov < req->iovec_count)
    {
      uint64_t next_sz = *(uint64_t *)(((uint8_t *)req->iov[curr_iov].iov_base)
                                       + curr_iov_off);
      if ((req->iov[curr_iov].iov_len > curr_iov_off) && next_sz)
        {
          req->next_iov = curr_iov;
          req->next_offset = curr_iov_off;
        }
      else
        {
          req->next_iov = 0;
          req->next_offset = 0;
        }
    }

  if (ok)
    {
      return SUCCESS_CODE;
    }
  free (dest_ptr);
  dest_ptr = NULL;
  *dest = NULL;
  *len = 0;
  req->next_iov = 0;
  req->next_offset = 0;
  for (size_t i = curr_iov; i < req->iovec_count; ++i)
    {
      for (size_t j = curr_iov_off; j < req->iov[i].iov_len; ++j)
        {
          uint8_t foot = *((uint8_t *)req->iov[i].iov_base) + j;
          if (foot == 0)
            {
              req->next_iov = i;
              req->next_offset = (j + 1) % req->iov[i].iov_len;
              return ERROR_CODE;
            }
        }
    }
  return ERROR_CODE;
}

static inline void
handle_read (struct saurion *const s, struct request *const req)
{
  void *msg = NULL;
  size_t len = 0;
  while (1)
    {
      if (!read_chunk (&msg, &len, req))
        {
          break;
        }
      if (req->next_iov || req->next_offset)
        {
          if (s->cb.on_readed && msg)
            {
              s->cb.on_readed (req->client_socket, msg, len,
                               s->cb.on_readed_arg);
            }
          free (msg);
          msg = NULL;
          continue;
        }
      if (req->prev && req->prev_size && req->prev_remain)
        {
          add_read_continue (s, req, next (s));
          return;
        }
      if (s->cb.on_readed && msg)
        {
          s->cb.on_readed (req->client_socket, msg, len, s->cb.on_readed_arg);
        }
      free (msg);
      msg = NULL;
      break;
    }
  add_read (s, req->client_socket);
}

static inline void
handle_write (const struct saurion *const s, const int fd)
{
  if (s->cb.on_wrote)
    {
      s->cb.on_wrote (fd, s->cb.on_wrote_arg);
    }
}

static inline void
handle_error (const struct saurion *const s, const struct request *const req)
{
  if (s->cb.on_error)
    {
      const char *resp = "ERROR";
      s->cb.on_error (req->client_socket, resp, (ssize_t)strlen (resp),
                      s->cb.on_error_arg);
    }
}

static inline void
handle_close (const struct saurion *const s, const struct request *const req)
{
  if (s->cb.on_closed)
    {
      s->cb.on_closed (req->client_socket, s->cb.on_closed_arg);
    }
  close (req->client_socket);
}

/******************* INTERFACE *******************/
int
saurion_set_socket (const int p)
{
  int sock = 0;
  struct sockaddr_in srv_addr;

  sock = socket (PF_INET, SOCK_STREAM, 0);
  if (sock < 1)
    {
      return ERROR_CODE;
    }

  int enable = 1;
  if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof (int)) < 0)
    {
      return ERROR_CODE;
    }

  memset (&srv_addr, 0, sizeof (srv_addr));
  srv_addr.sin_family = AF_INET;
  srv_addr.sin_port = htons (p);
  srv_addr.sin_addr.s_addr = htonl (INADDR_ANY);

  if (bind (sock, (const struct sockaddr *)&srv_addr, sizeof (srv_addr)) < 0)
    {
      return ERROR_CODE;
    }

  if (listen (sock, ACCEPT_QUEUE) < 0)
    {
      return ERROR_CODE;
    }

  return sock;
}

[[nodiscard]]
struct saurion *
saurion_create (uint32_t n_threads)
{
  LOG_INIT (" ");
  struct saurion *p = (struct saurion *)malloc (sizeof (struct saurion));
  if (!p)
    {
      LOG_END (" ");
      return NULL;
    }
  int ret = 0;
  ret = pthread_mutex_init (&p->status_m, NULL);
  if (ret)
    {
      free (p);
      LOG_END (" ");
      return NULL;
    }
  ret = pthread_cond_init (&p->status_c, NULL);
  if (ret)
    {
      free (p);
      LOG_END (" ");
      return NULL;
    }
  p->m_rings
      = (pthread_mutex_t *)malloc (n_threads * sizeof (pthread_mutex_t));
  if (!p->m_rings)
    {
      free (p);
      LOG_END (" ");
      return NULL;
    }
  for (uint32_t i = 0; i < n_threads; ++i)
    {
      pthread_mutex_init (&(p->m_rings[i]), NULL);
    }
  p->ss = 0;
  n_threads = (n_threads < 2 ? 2 : n_threads);
  n_threads = (n_threads > NUM_CORES ? NUM_CORES : n_threads);
  p->n_threads = n_threads;
  p->status = 0;
  p->list = NULL;
  p->cb.on_connected = NULL;
  p->cb.on_connected_arg = NULL;
  p->cb.on_readed = NULL;
  p->cb.on_readed_arg = NULL;
  p->cb.on_wrote = NULL;
  p->cb.on_wrote_arg = NULL;
  p->cb.on_closed = NULL;
  p->cb.on_closed_arg = NULL;
  p->cb.on_error = NULL;
  p->cb.on_error_arg = NULL;
  p->next = 0;
  p->efds = (int *)malloc (sizeof (int) * p->n_threads);
  if (!p->efds)
    {
      free (p->m_rings);
      free (p);
      LOG_END (" ");
      return NULL;
    }
  for (uint32_t i = 0; i < p->n_threads; ++i)
    {
      p->efds[i] = eventfd (0, EFD_NONBLOCK);
      if (p->efds[i] == ERROR_CODE)
        {
          for (uint32_t j = 0; j < i; ++j)
            {
              close (p->efds[j]);
            }
          free (p->efds);
          free (p->m_rings);
          free (p);
          LOG_END (" ");
          return NULL;
        }
    }
  p->rings
      = (struct io_uring *)malloc (sizeof (struct io_uring) * p->n_threads);
  if (!p->rings)
    {
      for (uint32_t j = 0; j < p->n_threads; ++j)
        {
          close (p->efds[j]);
        }
      free (p->efds);
      free (p->m_rings);
      free (p);
      LOG_END (" ");
      return NULL;
    }
  for (uint32_t i = 0; i < p->n_threads; ++i)
    {
      memset (&p->rings[i], 0, sizeof (struct io_uring));
      ret = io_uring_queue_init (SAURION_RING_SIZE, &p->rings[i], 0);
      if (ret)
        {
          for (uint32_t j = 0; j < p->n_threads; ++j)
            {
              close (p->efds[j]);
            }
          free (p->efds);
          free (p->rings);
          free (p->m_rings);
          free (p);
          LOG_END (" ");
          return NULL;
        }
    }
  p->pool = threadpool_create (p->n_threads);
  LOG_END (" ");
  return p;
}

static inline void
handle_event_read (const struct io_uring_cqe *const cqe,
                   struct saurion *const s, struct request *req)
{
  if (cqe->res < 0)
    {
      handle_error (s, req);
    }
  if (cqe->res < 1)
    {
      handle_close (s, req);
    }
  if (cqe->res > 0)
    {
      handle_read (s, req);
    }
  list_delete_node (&s->list, req);
}

[[nodiscard]]
static int
saurion_worker_master_loop_it (struct saurion *const s,
                               struct sockaddr_in *client_addr,
                               socklen_t *client_addr_len)
{
  LOG_INIT (" ");
  struct io_uring ring = s->rings[0];
  struct io_uring_cqe *cqe = NULL;
  int ret = io_uring_wait_cqe (&ring, &cqe);
  if (ret < 0)
    {
      free (cqe);
      LOG_END (" ");
      return CRITICAL_CODE;
    }
  struct request *req = (struct request *)cqe->user_data;
  if (!req)
    {
      io_uring_cqe_seen (&s->rings[0], cqe);
      LOG_END (" ");
      return SUCCESS_CODE;
    }
  if (cqe->res < 0)
    {
      list_delete_node (&s->list, req);
      LOG_END (" ");
      return CRITICAL_CODE;
    }
  if (req->client_socket == s->efds[0])
    {
      io_uring_cqe_seen (&s->rings[0], cqe);
      list_delete_node (&s->list, req);
      LOG_END (" ");
      return ERROR_CODE;
    }
  io_uring_cqe_seen (&s->rings[0], cqe);
  switch (req->event_type)
    {
    case EV_ACC:
      handle_accept (s, cqe->res);
      add_accept (s, client_addr, client_addr_len);
      add_read (s, cqe->res);
      list_delete_node (&s->list, req);
      break;
    case EV_REA:
      handle_event_read (cqe, s, req);
      break;
    case EV_WRI:
      handle_write (s, req->client_socket);
      list_delete_node (&s->list, req);
      break;
    }
  LOG_END (" ");
  return SUCCESS_CODE;
}

void
saurion_worker_master (void *arg)
{
  LOG_INIT (" ");
  struct saurion *const s = (struct saurion *)arg;
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof (client_addr);

  add_efd (s, s->efds[0], 0);
  add_accept (s, &client_addr, &client_addr_len);

  pthread_mutex_lock (&s->status_m);
  ++s->status;
  pthread_cond_broadcast (&s->status_c);
  pthread_mutex_unlock (&s->status_m);
  while (1)
    {
      int ret
          = saurion_worker_master_loop_it (s, &client_addr, &client_addr_len);
      if (ret == ERROR_CODE || ret == CRITICAL_CODE)
        {
          break;
        }
    }
  pthread_mutex_lock (&s->status_m);
  --s->status;
  pthread_cond_signal (&s->status_c);
  pthread_mutex_unlock (&s->status_m);
  LOG_END (" ");
  return;
}

[[nodiscard]]
static int
saurion_worker_slave_loop_it (struct saurion *const s, const int sel)
{
  LOG_INIT (" ");
  struct io_uring ring = s->rings[sel];
  struct io_uring_cqe *cqe = NULL;

  add_efd (s, s->efds[sel], sel);
  int ret = io_uring_wait_cqe (&ring, &cqe);
  if (ret < 0)
    {
      free (cqe);
      LOG_END (" ");
      return CRITICAL_CODE;
    }
  struct request *req = (struct request *)cqe->user_data;
  if (!req)
    {
      io_uring_cqe_seen (&ring, cqe);
      LOG_END (" ");
      return SUCCESS_CODE;
    }
  if (cqe->res < 0)
    {
      list_delete_node (&s->list, req);
      LOG_END (" ");
      return CRITICAL_CODE;
    }
  if (req->client_socket == s->efds[sel])
    {
      io_uring_cqe_seen (&ring, cqe);
      list_delete_node (&s->list, req);
      LOG_END (" ");
      return ERROR_CODE;
    }
  io_uring_cqe_seen (&ring, cqe);
  switch (req->event_type)
    {
    case EV_REA:
      handle_event_read (cqe, s, req);
      break;
    case EV_WRI:
      handle_write (s, req->client_socket);
      list_delete_node (&s->list, req);
      break;
    }
  LOG_END (" ");
  return SUCCESS_CODE;
}

void
saurion_worker_slave (void *arg)
{
  LOG_INIT (" ");
  struct saurion_wrapper *const ss = (struct saurion_wrapper *)arg;
  struct saurion *s = ss->s;
  const int sel = ss->sel;
  free (ss);

  add_efd (s, s->efds[sel], sel);

  pthread_mutex_lock (&s->status_m);
  ++s->status;
  pthread_cond_broadcast (&s->status_c);
  pthread_mutex_unlock (&s->status_m);
  while (1)
    {
      int res = saurion_worker_slave_loop_it (s, sel);
      if (res == ERROR_CODE || res == CRITICAL_CODE)
        {
          break;
        }
    }
  pthread_mutex_lock (&s->status_m);
  --s->status;
  pthread_cond_signal (&s->status_c);
  pthread_mutex_unlock (&s->status_m);
  LOG_END (" ");
  return;
}

[[nodiscard]]
int
saurion_start (struct saurion *const s)
{
  threadpool_init (s->pool);
  threadpool_add (s->pool, saurion_worker_master, s);
  struct saurion_wrapper *ss = NULL;
  for (uint32_t i = 1; i < s->n_threads; ++i)
    {
      ss = (struct saurion_wrapper *)malloc (sizeof (struct saurion_wrapper));
      if (!ss)
        {
          return ERROR_CODE;
        }
      ss->s = s;
      ss->sel = i;
      threadpool_add (s->pool, saurion_worker_slave, ss);
    }
  pthread_mutex_lock (&s->status_m);
  while (s->status < (int)s->n_threads)
    {
      pthread_cond_wait (&s->status_c, &s->status_m);
    }
  pthread_mutex_unlock (&s->status_m);
  return SUCCESS_CODE;
}

void
saurion_stop (const struct saurion *const s)
{
  uint64_t u = 1;
  for (uint32_t i = 0; i < s->n_threads; ++i)
    {
      while (write (s->efds[i], &u, sizeof (u)) < 0)
        {
          nanosleep (&TIMEOUT_RETRY_SPEC, NULL);
        }
    }
  threadpool_wait_empty (s->pool);
}

void
saurion_destroy (struct saurion *const s)
{
  pthread_mutex_lock (&s->status_m);
  while (s->status > 0)
    {
      pthread_cond_wait (&s->status_c, &s->status_m);
    }
  pthread_mutex_unlock (&s->status_m);
  threadpool_destroy (s->pool);
  for (uint32_t i = 0; i < s->n_threads; ++i)
    {
      io_uring_queue_exit (&s->rings[i]);
      pthread_mutex_destroy (&s->m_rings[i]);
    }
  free (s->m_rings);
  list_free (&s->list);
  for (uint32_t i = 0; i < s->n_threads; ++i)
    {
      close (s->efds[i]);
    }
  free (s->efds);
  if (!s->ss)
    {
      close (s->ss);
    }
  free (s->rings);
  pthread_mutex_destroy (&s->status_m);
  pthread_cond_destroy (&s->status_c);
  free (s);
}

void
saurion_send (struct saurion *const s, const int fd, const char *const msg)
{
  add_write (s, fd, msg, next (s));
}
