#include "low_saurion.h"

#include <liburing.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include "config.h"
#include "linked_list.h"
#include "logger.h"

#define EVENT_TYPE_ACCEPT 0  //! @brief Tipo de evento para aceptar una nueva conexión.
#define EVENT_TYPE_READ 1    //! @brief Tipo de evento para leer datos.
#define EVENT_TYPE_WRITE 2   //! @brief Tipo de evento para escribir datos.
#define EVENT_TYPE_WAIT 3    //! @brief Tipo de evento para esperar.
#define EVENT_TYPE_ERROR 4   //! @brief Tipo de evento para indicar un error.

struct request {
  void *prev;
  size_t prev_size;
  size_t prev_remain;
  int next_iov;
  size_t next_offset;
  int event_type;
  int iovec_count;
  int client_socket;
  struct iovec iov[];
};

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

struct saurion_wrapper {
  struct saurion *s;
  uint32_t sel;
};

static uint32_t next(struct saurion *const s) {
  s->next = (s->next + 1) % s->n_threads;
  return s->next;
}

static void free_request(struct request *req, void **children_ptr, const size_t amount) {
  if (children_ptr) {
    free(children_ptr);
    children_ptr = NULL;
  }
  for (size_t i = 0; i < amount; ++i) {
    free(req->iov[i].iov_base);
  }
  free(req);
  req = NULL;
}

[[nodiscard]] static int init_iovec(struct iovec *iov, size_t a, size_t pos, const void *const msg,
                                    const uint8_t h) {
  if (!iov || !iov->iov_base) {
    return ERROR_CODE;
  }
  if (msg) {
    size_t len = iov->iov_len;
    if (len > CHUNK_SZ) {
      return ERROR_CODE;
    }
    size_t offset_s = pos * CHUNK_SZ;
    size_t offset_iov = 0;
    if (h) {
      offset_s -= (pos ? 8 : 0);
      offset_iov += (!pos ? 8 : 0);
      if (pos == (a - 1)) {
        --len;
      }
    }
    char *dest = (char *)iov->iov_base + offset_iov;
    char *orig = (char *)msg + offset_s;
    memcpy(dest, orig, len - offset_iov);
    /*! TODO: Es eliminable esta comprobación
     *  \todo Es eliminable esta comprobación
     */
    if (len < CHUNK_SZ) {
      memset(dest + len, 0, CHUNK_SZ - (len + offset_iov));
    }
  } else {
    memset((char *)iov->iov_base, 0, CHUNK_SZ);
  }
  return SUCCESS_CODE;
}

[[nodiscard]] static int alloc_iovec(struct iovec *iov, const size_t a, const size_t p,
                                     const size_t s, void **c_p) {
  iov->iov_base = malloc(CHUNK_SZ);
  if (!iov->iov_base) {
    return ERROR_CODE;
  }
  iov->iov_len = (p == (a - 1) ? (s % CHUNK_SZ) : CHUNK_SZ);
  if (iov->iov_len == 0) {
    iov->iov_len = CHUNK_SZ;
  }
  c_p[p] = iov->iov_base;
  return SUCCESS_CODE;
}

[[nodiscard]] static int s_req(struct request **r, struct Node **l, size_t s, const void *const m,
                               const uint8_t h) {
  if (h) {
    s += (8 + 1);
  }
  size_t amount = s / CHUNK_SZ;
  amount = amount + (s % CHUNK_SZ == 0 ? 0 : 1);
  struct request *temp =
      (struct request *)malloc(sizeof(struct request) + sizeof(struct iovec) * amount);
  if (!temp) {
    return ERROR_CODE;
  }
  if (!*r) {
    *r = temp;
    (*r)->prev = NULL;
    (*r)->prev_size = 0;
    (*r)->prev_remain = 0;
    (*r)->next_iov = 0;
    (*r)->next_offset = 0;
  } else {
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
  void **children_ptr = (void **)malloc(amount * sizeof(void *));
  if (!children_ptr) {
    return ERROR_CODE;
  }
  for (size_t i = 0; i < amount; ++i) {
    if (!alloc_iovec(&req->iov[i], amount, i, s, children_ptr)) {
      free_request(req, children_ptr, amount);
      free(children_ptr);
      return ERROR_CODE;
    }
    if (!init_iovec(&req->iov[i], amount, i, m, h)) {
      free_request(req, children_ptr, amount);
      free(children_ptr);
      return ERROR_CODE;
    }
  }
  if (list_insert(l, req, amount, children_ptr)) {
    free_request(req, children_ptr, amount);
    free(children_ptr);
    return ERROR_CODE;
  }
  free(children_ptr);
  return SUCCESS_CODE;
}

/*********************************** ADDERS ***********************************/
static void add_accept(struct saurion *const s, const struct sockaddr_in *const ca,
                       socklen_t *const cal) {
  int res = ERROR_CODE;
  pthread_mutex_lock(&s->m_rings[0]);
  while (res != SUCCESS_CODE) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&s->rings[0]);
    while (!sqe) {
      sqe = io_uring_get_sqe(&s->rings[0]);
      usleep(TIMEOUT_RETRY);
    }
    struct request *req = NULL;
    if (!s_req(&req, &s->list, 0, NULL, 0)) {
      free(sqe);
      usleep(TIMEOUT_RETRY);
      res = ERROR_CODE;
      continue;
    }
    req->client_socket = 0;
    req->event_type = EVENT_TYPE_ACCEPT;
    io_uring_prep_accept(sqe, s->ss, (struct sockaddr *)ca, cal, 0);
    io_uring_sqe_set_data(sqe, req);
    if (io_uring_submit(&s->rings[0]) < 0) {
      free(sqe);
      list_delete_node(&s->list, req);
      usleep(TIMEOUT_RETRY);
      res = ERROR_CODE;
      continue;
    }
    res = SUCCESS_CODE;
  }
  pthread_mutex_unlock(&s->m_rings[0]);
}

static void add_efd(struct saurion *const s, const int client_socket, const int sel) {
  pthread_mutex_lock(&s->m_rings[sel]);
  int res = ERROR_CODE;
  while (res != SUCCESS_CODE) {
    struct io_uring *ring = &s->rings[sel];
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    while (!sqe) {
      sqe = io_uring_get_sqe(ring);
      usleep(TIMEOUT_RETRY);
    }
    struct request *req = NULL;
    if (!s_req(&req, &s->list, CHUNK_SZ, NULL, 0)) {
      free(sqe);
      res = ERROR_CODE;
      continue;
    }
    req->event_type = EVENT_TYPE_READ;
    req->client_socket = client_socket;
    io_uring_prep_readv(sqe, client_socket, &req->iov[0], req->iovec_count, 0);
    io_uring_sqe_set_data(sqe, req);
    if (io_uring_submit(ring) < 0) {
      free(sqe);
      list_delete_node(&s->list, req);
      res = ERROR_CODE;
      continue;
    }
    res = SUCCESS_CODE;
  }
  pthread_mutex_unlock(&s->m_rings[sel]);
}

static void add_read(struct saurion *const s, const int client_socket) {
  int sel = next(s);
  int res = ERROR_CODE;
  pthread_mutex_lock(&s->m_rings[sel]);
  while (res != SUCCESS_CODE) {
    struct io_uring *ring = &s->rings[sel];
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    while (!sqe) {
      sqe = io_uring_get_sqe(ring);
      usleep(TIMEOUT_RETRY);
    }
    struct request *req = NULL;
    if (!s_req(&req, &s->list, CHUNK_SZ, NULL, 0)) {
      free(sqe);
      res = ERROR_CODE;
      continue;
    }
    req->event_type = EVENT_TYPE_READ;
    req->client_socket = client_socket;
    io_uring_prep_readv(sqe, client_socket, &req->iov[0], req->iovec_count, 0);
    io_uring_sqe_set_data(sqe, req);
    if (io_uring_submit(ring) < 0) {
      free(sqe);
      list_delete_node(&s->list, req);
      res = ERROR_CODE;
      continue;
    }
    res = SUCCESS_CODE;
  }
  pthread_mutex_unlock(&s->m_rings[sel]);
}

static void add_read_continue(struct saurion *const s, struct request *oreq, const int sel) {
  pthread_mutex_lock(&s->m_rings[sel]);
  int res = ERROR_CODE;
  while (res != SUCCESS_CODE) {
    struct io_uring *ring = &s->rings[sel];
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    while (!sqe) {
      sqe = io_uring_get_sqe(ring);
      usleep(TIMEOUT_RETRY);
    }
    if (!s_req(&oreq, &s->list, oreq->prev_remain, NULL, 0)) {
      free(sqe);
      res = ERROR_CODE;
      continue;
    }
    io_uring_prep_readv(sqe, oreq->client_socket, &oreq->iov[0], oreq->iovec_count, 0);
    io_uring_sqe_set_data(sqe, oreq);
    if (io_uring_submit(ring) < 0) {
      free(sqe);
      list_delete_node(&s->list, oreq);
      res = ERROR_CODE;
      continue;
    }
    res = SUCCESS_CODE;
  }
  pthread_mutex_unlock(&s->m_rings[sel]);
}

static void add_write(struct saurion *const s, const int fd, const void *const str, const int sel) {
  int res = ERROR_CODE;
  pthread_mutex_lock(&s->m_rings[sel]);
  while (res != SUCCESS_CODE) {
    struct io_uring *ring = &s->rings[sel];
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    while (!sqe) {
      sqe = io_uring_get_sqe(ring);
      usleep(TIMEOUT_RETRY);
    }
    struct request *req = NULL;
    if (!s_req(&req, &s->list, strlen(str), (void *)str, 1)) {
      free(sqe);
      res = ERROR_CODE;
      continue;
    }
    req->event_type = EVENT_TYPE_WRITE;
    req->client_socket = fd;
    io_uring_prep_writev(sqe, req->client_socket, req->iov, req->iovec_count, 0);
    io_uring_sqe_set_data(sqe, req);
    if (io_uring_submit(ring) < 0) {
      free(sqe);
      list_delete_node(&s->list, req);
      res = ERROR_CODE;
      usleep(TIMEOUT_RETRY);
      continue;
    }
    res = SUCCESS_CODE;
  }
  pthread_mutex_unlock(&s->m_rings[sel]);
}

/*********************************** HANDLERS ***********************************/
static void handle_accept(const struct saurion *const s, const int fd) {
  LOG_INIT("");
  if (s->cb.on_connected) {
    s->cb.on_connected(fd, s->cb.on_connected_arg);
  }
  LOG_END("");
}

static void read_chunk(void **dest, size_t *len, struct request *const req) {
  // Verificación inicial
  if (req->iovec_count == 0) {
    return;
  }
  // Inicialización
  size_t msg_size, o_s = 0, o_iov = req->next_offset;
  if (req->prev && req->prev_size && req->prev_remain) {  // Mensaje previo no completado
    msg_size = req->prev_size;
    o_s = req->prev_size - req->prev_remain;
    *dest = req->prev;
  } else if (req->next_iov || req->next_offset) {  // Hay varios mensajes leyendo el último
    msg_size = *(size_t *)((char *)req->iov[req->next_iov].iov_base + req->next_offset);
    o_s = 0;
    *dest = malloc(msg_size + 1);
    if (!*dest) {
      return;
    }
    ((char *)*dest)[msg_size] = 0;
    o_iov += sizeof(size_t);
  } else {  // Leyendo el primer mensaje
    msg_size = *(size_t *)((char *)req->iov[0].iov_base);
    o_s = 0;
    if (msg_size > req->iov[0].iov_len) {
      req->prev = malloc(msg_size);
      if (!req->prev) {
        return;
      }
      *dest = req->prev;
      req->prev_size = msg_size;
      req->prev_remain = msg_size - (req->iov[0].iov_len - 8);
    } else {
      *dest = malloc(msg_size + 1);
      if (!*dest) {
        return;
      }
      ((char *)*dest)[msg_size] = 0;
    }
    o_iov += sizeof(size_t);
  }
  // Bucle principal de copia
  char *src;
  size_t readed = 0, curr_len = 0;
  int i = 0;
  for (i = req->next_iov; i < req->iovec_count; ++i) {
    src = (char *)req->iov[i].iov_base + o_iov;
    curr_len = MIN(msg_size - o_s, req->iov[i].iov_len - o_iov);
    memcpy((char *)*dest + o_s, src, curr_len);
    readed += curr_len + 9;
    o_s += curr_len;
    o_iov = 0;
  }
  // Actualización del estado
  --i;
  i = MAX(i, 0);
  int has_prev = req->prev && req->prev_size;
  int has_next = ((req->next_offset + readed) < req->iov[i].iov_len) ? 1 : 0;
  *len = msg_size;
  if (has_prev) {
    req->prev = *dest;
    req->prev_size = msg_size;
    req->prev_remain = req->prev_size - o_s;
    *len = 0;
  }
  if (has_next) {
    req->next_iov = i;
    req->next_offset += curr_len + 9;
  } else {
    req->next_iov = 0;
    req->next_offset = 0;
  }
}

static void handle_read(struct saurion *const s, struct request *const req, int bytes) {
  LOG_INIT("");
  void *msg = NULL;
  int last_bytes = bytes % CHUNK_SZ;
  last_bytes = (!last_bytes ? CHUNK_SZ : last_bytes);
  req->iov[MAX(req->iovec_count - 1, 0)].iov_len = last_bytes;
  size_t len = 0;
  while (1) {
    read_chunk(&msg, &len, req);
    // Hay siguiente
    if (req->next_iov || req->next_offset) {
      if (s->cb.on_readed && msg) {
        s->cb.on_readed(req->client_socket, msg, len, s->cb.on_readed_arg);
      }
      free(msg);
      msg = NULL;
      continue;
    }
    // Hay previo y se ha completado
    if (req->prev && req->prev_size && !req->prev_remain) {
      if (s->cb.on_readed) {
        s->cb.on_readed(req->client_socket, req->prev, req->prev_size, s->cb.on_readed_arg);
      }
      free(req->prev);
      req->prev_size = 0;
      req->prev_remain = 0;
      req->prev = NULL;
      // Hay siguiente
      if (req->next_iov || req->next_offset) {
        continue;
      }
      // No hay siguiente
      break;
    }
    // Hay previo pero no se ha completado
    if (req->prev && req->prev_size && req->prev_remain) {
      add_read_continue(s, req, next(s));
      LOG_END("");
      return;
    }
    // Hay un único mensaje y se ha completado
    if (s->cb.on_readed && msg) {
      s->cb.on_readed(req->client_socket, msg, strlen(msg), s->cb.on_readed_arg);
    }
    free(msg);
    msg = NULL;
    break;
  }
  add_read(s, req->client_socket);
  LOG_END("");
}

static void handle_write(const struct saurion *const s, const int fd) {
  LOG_INIT("");
  if (s->cb.on_wrote) {
    s->cb.on_wrote(fd, s->cb.on_wrote_arg);
  }
  LOG_END("");
}

static void handle_error(const struct saurion *const s, const struct request *const req) {
  LOG_INIT("");
  if (s->cb.on_error) {
    const char *resp = "ERROR";
    s->cb.on_error(req->client_socket, resp, (ssize_t)strlen(resp), s->cb.on_error_arg);
  }
  LOG_END("");
}

static void handle_close(const struct saurion *const s, const struct request *const req) {
  LOG_INIT("");
  if (s->cb.on_closed) {
    s->cb.on_closed(req->client_socket, s->cb.on_closed_arg);
  }
  close(req->client_socket);
  LOG_END("");
}

/*********************************** INTERFACE ***********************************/
int EXTERNAL_set_socket(const int p) {
  int sock = 0;
  struct sockaddr_in srv_addr;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == ERROR_CODE) {
    return ERROR_CODE;
  }

  int enable = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
    return ERROR_CODE;
  }

  memset(&srv_addr, 0, sizeof(srv_addr));
  srv_addr.sin_family = AF_INET;
  srv_addr.sin_port = htons(p);
  srv_addr.sin_addr.s_addr = INADDR_ANY;

  /* We bind to a port and turn this socket into a listening
   * socket.
   * */
  if (bind(sock, (const struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
    close(sock);
    return ERROR_CODE;
  }

  if (listen(sock, ACCEPT_QUEUE) < 0) {
    close(sock);
    return ERROR_CODE;
  }
  int flags = fcntl(sock, F_GETFL, 0);
  fcntl(sock, F_SETFL, flags | O_NONBLOCK);

  return (sock);
}

[[nodiscard]] struct saurion *saurion_create(uint32_t n_threads) {
  LOG_INIT("");
  // Asignar memoria
  struct saurion *p = (struct saurion *)malloc(sizeof(struct saurion));
  if (!p) {
    LOG_END("");
    return NULL;
  }
  // Inicializar mutex
  int ret = 0;
  ret = pthread_mutex_init(&p->status_m, NULL);
  if (ret) {
    free(p);
    LOG_END("");
    return NULL;
  }
  ret = pthread_cond_init(&p->status_c, NULL);
  if (ret) {
    free(p);
    LOG_END("");
    return NULL;
  }
  n_threads = (n_threads < 2 ? 2 : n_threads);
  n_threads = (n_threads > NUM_CORES ? NUM_CORES : n_threads);
  p->m_rings = (pthread_mutex_t *)malloc(n_threads * sizeof(pthread_mutex_t));
  if (!p->m_rings) {
    free(p);
    LOG_END("");
    return NULL;
  }
  for (uint32_t i = 0; i < n_threads; ++i) {
    pthread_mutex_init(&(p->m_rings[i]), NULL);
  }
  // Inicializar miembros
  p->ss = 0;
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
  // Inicializar efds
  p->efds = (int *)malloc(sizeof(int) * p->n_threads);
  if (!p->efds) {
    free(p->m_rings);
    free(p);
    LOG_END("");
    return NULL;
  }
  for (uint32_t i = 0; i < p->n_threads; ++i) {
    p->efds[i] = eventfd(0, EFD_NONBLOCK);
    if (p->efds[i] == ERROR_CODE) {
      for (uint32_t j = 0; j < i; ++j) {
        close(p->efds[j]);
      }
      free(p->efds);
      free(p->m_rings);
      free(p);
      LOG_END("");
      return NULL;
    }
  }
  // Inicializar rings
  p->rings = (struct io_uring *)malloc(sizeof(struct io_uring) * p->n_threads);
  if (!p->rings) {
    for (uint32_t j = 0; j < p->n_threads; ++j) {
      close(p->efds[j]);
    }
    free(p->efds);
    free(p->m_rings);
    free(p);
    LOG_END("");
    return NULL;
  }
  for (uint32_t i = 0; i < p->n_threads; ++i) {
    memset(&p->rings[i], 0, sizeof(struct io_uring));
    ret = io_uring_queue_init(SAURION_RING_SIZE, &p->rings[i], 0);
    if (ret) {
      for (uint32_t j = 0; j < p->n_threads; ++j) {
        close(p->efds[j]);
      }
      free(p->efds);
      free(p->rings);
      free(p->m_rings);
      free(p);
      LOG_END("");
      return NULL;
    }
  }
  p->pool = ThreadPool_create(p->n_threads);
  LOG_END("");
  return p;
}

void saurion_worker_master(void *arg) {
  LOG_INIT("");
  struct saurion *const s = (struct saurion *)arg;
  struct io_uring ring = s->rings[0];
  struct io_uring_cqe *cqe = NULL;
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);

  add_efd(s, s->efds[0], 0);
  add_accept(s, &client_addr, &client_addr_len);

  pthread_mutex_lock(&s->status_m);
  s->status = 1;
  pthread_cond_signal(&s->status_c);
  pthread_mutex_unlock(&s->status_m);
  while (1) {
    int ret = io_uring_wait_cqe(&ring, &cqe);
    if (ret < 0) {
      free(cqe);
      LOG_END("");
      return;
    }
    struct request *req = (struct request *)cqe->user_data;  // NOLINT(performance-no-int-to-ptr)
    if (!req) {
      io_uring_cqe_seen(&s->rings[0], cqe);
      continue;
    }
    if (cqe->res < 0) {
      handle_error(s, req);
      handle_close(s, req);
      free(req);
      LOG_END("");
      return;
    }
    if (req->client_socket == s->efds[0]) {
      io_uring_cqe_seen(&s->rings[0], cqe);
      list_delete_node(&s->list, req);
      break;
    }
    /* Mark this request as processed */
    io_uring_cqe_seen(&s->rings[0], cqe);
    switch (req->event_type) {
      case EVENT_TYPE_ACCEPT:
        handle_accept(s, cqe->res);
        add_accept(s, &client_addr, &client_addr_len);
        add_read(s, cqe->res);
        list_delete_node(&s->list, req);
        break;
      case EVENT_TYPE_READ:
        if (cqe->res < 1) {
          handle_close(s, req);
        }
        if (cqe->res > 0) {
          handle_read(s, req, cqe->res);
        }
        list_delete_node(&s->list, req);
        break;
      case EVENT_TYPE_WRITE:
        handle_write(s, req->client_socket);
        list_delete_node(&s->list, req);
        break;
    }
  }
  pthread_mutex_lock(&s->status_m);
  s->status = 2;
  pthread_cond_signal(&s->status_c);
  pthread_mutex_unlock(&s->status_m);
  LOG_END("");
  return;
}

void saurion_worker_slave(void *arg) {
  LOG_INIT("");
  struct saurion_wrapper *const ss = (struct saurion_wrapper *)arg;
  struct saurion *s = ss->s;
  const int sel = ss->sel;
  free(ss);
  struct io_uring ring = s->rings[sel];
  struct io_uring_cqe *cqe = NULL;
  add_efd(s, s->efds[sel], sel);
  pthread_mutex_lock(&s->status_m);
  s->status = 1;
  pthread_cond_signal(&s->status_c);
  pthread_mutex_unlock(&s->status_m);
  while (1) {
    int ret = io_uring_wait_cqe(&ring, &cqe);
    if (ret < 0) {
      free(cqe);
      LOG_END("");
      return;
    }
    struct request *req = (struct request *)cqe->user_data;
    if (!req) {
      io_uring_cqe_seen(&ring, cqe);
      continue;
    }
    if (cqe->res < 0) {
      handle_error(s, req);
      handle_close(s, req);
      free(req);
      LOG_END("");
      return;
    }
    if (req->client_socket == s->efds[sel]) {
      io_uring_cqe_seen(&ring, cqe);
      list_delete_node(&s->list, req);
      break;
    }
    /* Mark this request as processed */
    io_uring_cqe_seen(&ring, cqe);
    switch (req->event_type) {
      case EVENT_TYPE_READ:
        if (cqe->res < 1) {
          handle_close(s, req);
        }
        if (cqe->res > 0) {
          handle_read(s, req, cqe->res);
        }
        list_delete_node(&s->list, req);
        break;
      case EVENT_TYPE_WRITE:
        handle_write(s, req->client_socket);
        list_delete_node(&s->list, req);
        break;
    }
  }
  pthread_mutex_lock(&s->status_m);
  s->status = 2;
  pthread_cond_signal(&s->status_c);
  pthread_mutex_unlock(&s->status_m);
  LOG_END("");
  return;
}

[[nodiscard]] int saurion_start(struct saurion *const s) {
  LOG_INIT("");
  ThreadPool_init(s->pool);
  ThreadPool_add_default(s->pool, saurion_worker_master, s);
  struct saurion_wrapper *ss = NULL;
  for (uint32_t i = 1; i < s->n_threads; ++i) {
    ss = (struct saurion_wrapper *)malloc(sizeof(struct saurion_wrapper));
    ss->s = s;
    ss->sel = i;
    ThreadPool_add_default(s->pool, saurion_worker_slave, ss);
  }
  LOG_INIT("");
  return SUCCESS_CODE;
}

void saurion_stop(const struct saurion *const s) {
  LOG_INIT("");
  uint64_t u = 1;
  for (uint32_t i = 0; i < s->n_threads; ++i) {
    while (write(s->efds[i], &u, sizeof(u)) < 0) {
      usleep(TIMEOUT_RETRY);
    }
  }
  ThreadPool_wait_empty(s->pool);
  LOG_END("");
}

void saurion_destroy(struct saurion *const s) {
  pthread_mutex_lock(&s->status_m);
  while (s->status == 1) {
    pthread_cond_wait(&s->status_c, &s->status_m);
  }
  pthread_mutex_unlock(&s->status_m);
  ThreadPool_destroy(s->pool);
  for (uint32_t i = 0; i < s->n_threads; ++i) {
    io_uring_queue_exit(&s->rings[i]);
    pthread_mutex_destroy(&s->m_rings[i]);
  }
  free(s->m_rings);
  list_free(&s->list);
  for (uint32_t i = 0; i < s->n_threads; ++i) {
    close(s->efds[i]);
  }
  free(s->efds);
  close(s->ss);
  free(s->rings);
  pthread_mutex_destroy(&s->status_m);
  pthread_cond_destroy(&s->status_c);
  free(s);
}

void saurion_send(struct saurion *const s, const int fd, const void *const msg) {
  add_write(s, fd, msg, next(s));
}
