#include "low_saurion.h"

#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "linked_list.h"

#define EVENT_TYPE_ACCEPT 0  //! @brief Tipo de evento para aceptar una nueva conexión.
#define EVENT_TYPE_READ 1    //! @brief Tipo de evento para leer datos.
#define EVENT_TYPE_WRITE 2   //! @brief Tipo de evento para escribir datos.
#define EVENT_TYPE_WAIT 3    //! @brief Tipo de evento para esperar.
#define EVENT_TYPE_ERROR 4   //! @brief Tipo de evento para indicar un error.
#define QUEUE_DEPTH 256
#define WAIT_NANOS 10

struct request {
  int event_type;
  int iovec_count;
  int client_socket;
  struct iovec iov[];
};

// NOLINTNEXTLINE (readability-function-cognitive-complexity)
void set_request(struct request **request, struct Node **list, size_t size, void *msg) {
  ++size;
  size_t amount = size / IOVEC_SZ;
  amount = amount + (size % IOVEC_SZ == 0 ? 0 : 1);
  *request = malloc(sizeof(struct request) + sizeof(struct iovec) * amount);
  struct request *req = *request;
  req->iovec_count = (int)amount;
  if (!req) {
    return;
  }
  void **children_ptr = NULL;
  if (size > 0) {
    children_ptr = (void **)malloc(sizeof(void *) * amount);
    if (!children_ptr) {
      free(req);
      return;
    }
    for (size_t i = 0; i < amount; ++i) {
      req->iov[i].iov_len = IOVEC_SZ;
      if (i == amount - 1) {
        req->iov[i].iov_len = size % IOVEC_SZ;
      }
      req->iov[i].iov_base = malloc(IOVEC_SZ);
      if (!req->iov[i].iov_base) {
        for (size_t j = 0; j < i; ++j) {
          free(req->iov[i].iov_base);
        }
        free(children_ptr);
        free(req);
        return;
      }
      children_ptr[i] = req->iov[i].iov_base;
    }
    size_t slen = IOVEC_SZ;
    // Inicializar iovecs
    if (msg) {
      for (size_t i = 0; i < amount; ++i) {
        if (i == amount - 1) {
          slen = (size - 1) % IOVEC_SZ;
        }
        memcpy((char *)req->iov[i].iov_base, (char *)msg + (i * IOVEC_SZ), slen);
        if (slen < IOVEC_SZ) {
          memset((char *)req->iov[i].iov_base + slen, 0, IOVEC_SZ - slen);
          memset((char *)req->iov[i].iov_base + slen, '\n', 1);
        }
      }
    } else {
      for (size_t i = 0; i < amount; ++i) {
        memset(req->iov[i].iov_base, 0, IOVEC_SZ);
      }
    }
  }
  if (list_insert(list, req, amount, children_ptr)) {
    if (children_ptr) {
      free(children_ptr);
    }
    for (size_t i = 0; i < amount; ++i) {
      free(req->iov[i].iov_base);
    }
    free(req);
    return;
  }
  if (children_ptr) {
    free(children_ptr);
  }
}

int add_accept(struct saurion *const s, const struct sockaddr_in *const ca, socklen_t *const cal) {
  struct io_uring_sqe *sqe = io_uring_get_sqe(&s->ring);
  while (!sqe) {
    sqe = io_uring_get_sqe(&s->ring);
    usleep(WAIT_NANOS);
  }
  struct request *req = NULL;
  set_request(&req, &s->list, 0, NULL);
  if (!req) {
    free(sqe);
    return 1;
  }
  req->client_socket = 0;
  req->event_type = EVENT_TYPE_ACCEPT;
  io_uring_prep_accept(sqe, s->ss, (struct sockaddr *)ca, cal, 0);
  io_uring_sqe_set_data(sqe, req);
  if (io_uring_submit(&s->ring) < 0) {
    free(sqe);
    list_delete_node(&s->list, req);
    return 1;
  }
  return 0;
}

int add_read(struct saurion *const s, const int client_socket) {
  struct io_uring_sqe *sqe = io_uring_get_sqe(&s->ring);
  while (!sqe) {
    sqe = io_uring_get_sqe(&s->ring);
    usleep(WAIT_NANOS);
  }
  struct request *req = NULL;
  set_request(&req, &s->list, IOVEC_SZ, NULL);
  if (!req) {
    free(sqe);
    return 1;
  }
  req->event_type = EVENT_TYPE_READ;
  req->client_socket = client_socket;
  io_uring_prep_readv(sqe, client_socket, &req->iov[0], req->iovec_count, 0);
  io_uring_sqe_set_data(sqe, req);
  if (io_uring_submit(&s->ring) < 0) {
    free(sqe);
    list_delete_node(&s->list, req);
    return 1;
  }
  return 0;
}

int add_write(struct saurion *const s, int fd, const char *const str) {
  struct io_uring_sqe *sqe = io_uring_get_sqe(&s->ring);
  while (!sqe) {
    sqe = io_uring_get_sqe(&s->ring);
    usleep(WAIT_NANOS);
  }
  struct request *req = NULL;
  set_request(&req, &s->list, strlen(str), (void *)str);
  req->event_type = EVENT_TYPE_WRITE;
  req->client_socket = fd;
  io_uring_prep_writev(sqe, req->client_socket, req->iov, req->iovec_count, 0);
  io_uring_sqe_set_data(sqe, req);
  if (io_uring_submit(&s->ring) < 0) {
    free(sqe);
    list_delete_node(&s->list, req);
    return 1;
  }
  return 0;
}

void handle_accept(const struct saurion *const s, const int fd) {
  if (s->cb.on_connected != NULL) {
    s->cb.on_connected(fd, s->cb.on_connected_arg);
  }
}

void handle_read(const struct saurion *const s, const struct request *const req, const size_t len) {
  char res[len];
  char *dp = res;
  for (int i = 0; i < req->iovec_count; ++i) {
    char *oip = (char *)req->iov[i].iov_base;
    char *ofp = oip + req->iov[i].iov_len;
    for (char *op = oip; op < ofp; ++op) {
      if (*op == '\n' || *op == '\0') {
        if (dp != res) {
          *dp = '\0';
          if (s->cb.on_readed != NULL) {
            s->cb.on_readed(req->client_socket, res, (dp - res), s->cb.on_readed_arg);
          }
          dp = res;
        }
        continue;
      }
      *dp = *op;
      ++dp;
    }
  }
}

void handle_write(const struct saurion *const s, const int fd) {
  if (s->cb.on_wrote != NULL) {
    s->cb.on_wrote(fd, s->cb.on_wrote_arg);
  }
}

void handle_error(const struct saurion *const s, const struct request *const req) {
  if (s->cb.on_error != NULL) {
    const char *resp = "ERROR";
    s->cb.on_error(req->client_socket, resp, (ssize_t)strlen(resp), s->cb.on_error_arg);
  }
}

void handle_close(const struct saurion *const s, const struct request *const req) {
  if (s->cb.on_closed != NULL) {
    s->cb.on_closed(req->client_socket, s->cb.on_closed_arg);
  }
}

/*********************************** PUBLIC ***********************************/
int EXTERNAL_set_socket(const int p) {
  int sock = 0;
  struct sockaddr_in srv_addr;

  sock = socket(PF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    return 0;
  }

  int enable = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
    return 0;
  }

  memset(&srv_addr, 0, sizeof(srv_addr));
  srv_addr.sin_family = AF_INET;
  srv_addr.sin_port = htons(p);
  srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  /* We bind to a port and turn this socket into a listening
   * socket.
   * */
  if (bind(sock, (const struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
    return 0;
  }

  if (listen(sock, ACCEPT_QUEUE) < 0) {
    return 0;
  }

  return (sock);
}

struct saurion *saurion_create(void) {
  struct saurion *p = (struct saurion *)malloc(sizeof(*p));
  if (!p) {
    return NULL;
  }
  p->efd = eventfd(0, EFD_NONBLOCK);
  if (p->efd == -1) {
    free(p);
    return NULL;
  }
  int ret = io_uring_queue_init(QUEUE_DEPTH, &p->ring, 0);
  if (ret) {
    free(p);
    return NULL;
  }
  p->list = NULL;
  add_read(p, p->efd);
  p->status = 0;
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
  ret = pthread_mutex_init(&p->status_m, NULL);
  if (ret) {
    free(p);
    return NULL;
  }
  pthread_cond_init(&p->status_c, NULL);
  if (ret) {
    free(p);
    return NULL;
  }
  return p;
}

int saurion_start(struct saurion *const s) {  // NOLINT(readability-function-cognitive-complexity)
  struct io_uring_cqe *cqe = NULL;
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);

  while (add_accept(s, &client_addr, &client_addr_len)) {
    usleep(WAIT_NANOS);
  }

  pthread_mutex_lock(&s->status_m);
  s->status = 1;
  pthread_cond_signal(&s->status_c);
  pthread_mutex_unlock(&s->status_m);
  while (1) {
    int ret = io_uring_wait_cqe(&s->ring, &cqe);
    if (ret < 0) {
      free(cqe);
      return 1;
    }
    struct request *req = (struct request *)cqe->user_data;  // NOLINT(performance-no-int-to-ptr)
    if (!req) {
      io_uring_cqe_seen(&s->ring, cqe);
      continue;
    }
    if (cqe->res < 0) {
      free(req);
      free(cqe);
      return 1;
    }
    if (req->client_socket == s->efd) {
      io_uring_cqe_seen(&s->ring, cqe);
      list_delete_node(&s->list, req);
      break;
    }
    switch (req->event_type) {
      case EVENT_TYPE_ACCEPT:
        handle_accept(s, cqe->res);
        while (add_accept(s, &client_addr, &client_addr_len)) {
          usleep(WAIT_NANOS);
        }
        while (add_read(s, cqe->res)) {
          usleep(WAIT_NANOS);
        }
        list_delete_node(&s->list, req);
        break;
      case EVENT_TYPE_READ:
        if (cqe->res < 0) {
          handle_error(s, req);
        }
        if (cqe->res < 1) {
          handle_close(s, req);
        }
        if (cqe->res > 0) {
          handle_read(s, req, cqe->res);
          while (add_read(s, req->client_socket)) {
            usleep(WAIT_NANOS);
          }
        }
        list_delete_node(&s->list, req);
        break;
      case EVENT_TYPE_WRITE:
        handle_write(s, req->client_socket);
        list_delete_node(&s->list, req);
        break;
    }
    /* Mark this request as processed */
    io_uring_cqe_seen(&s->ring, cqe);
  }
  pthread_mutex_lock(&s->status_m);
  s->status = 2;
  pthread_cond_signal(&s->status_c);
  pthread_mutex_unlock(&s->status_m);
  return 0;
}

void saurion_stop(const struct saurion *const s) {
  uint64_t u = 1;
  while (write(s->efd, &u, sizeof(u)) < 0) {
    usleep(WAIT_NANOS);
  }
}

void saurion_destroy(struct saurion *const s) {
  pthread_mutex_lock(&s->status_m);
  while (s->status == 1) {
    pthread_cond_wait(&s->status_c, &s->status_m);
  }
  pthread_mutex_unlock(&s->status_m);
  io_uring_queue_exit(&s->ring);
  list_free(&s->list);
  close(s->efd);
  close(s->ss);
  free(s);
}

void saurion_send(struct saurion *const s, const int fd, const char *const msg) {
  while (add_write(s, fd, msg)) {
    usleep(WAIT_NANOS);
  }
}
