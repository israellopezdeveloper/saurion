#include "low_saurion.h"

#include <bits/types/struct_iovec.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "gtest/gtest.h"
#include "low_saurion_secret.h"

void fill_with_alphabet(char **s, size_t length, uint8_t h) {
  const char *alphabet = "abcdefghijklmnopqrstuvwxyz";
  int alphabet_len = 26;

  size_t wrapper = sizeof(uint64_t) + sizeof(char);
  if (h) {
    size_t msg_size = length + wrapper;
    *s = (char *)malloc(msg_size);
    *(uint64_t *)(*s) = length;
    int pos = sizeof(uint64_t);
    for (size_t i = 0; i < length; i++) {
      (*s)[pos] = alphabet[i % alphabet_len];
      pos++;
    }
    (*s)[msg_size - 1] = 0;
  } else {
    *s = (char *)malloc(length + 1);
    int pos = 0;
    for (size_t i = 0; i < length; i++) {
      (*s)[pos] = alphabet[i % alphabet_len];
      pos++;
    }
    (*s)[length] = 0;
  }
}

TEST(Tools, alphabet_with_header) {
  char *str = NULL;
  size_t size = 4;
  fill_with_alphabet(&str, size, 1);
  EXPECT_NE(str, nullptr);
  uint64_t content_size = *(uint64_t *)str;
  char *str_content = str + sizeof(uint64_t);
  uint8_t foot = *(uint8_t *)(str + content_size);
  EXPECT_EQ(content_size, size);
  EXPECT_STREQ(str_content, "abcd");
  EXPECT_EQ(foot, 0);
  free(str);
}

TEST(Tools, alphabet_without_header) {
  char *str = NULL;
  size_t size = 4;
  fill_with_alphabet(&str, size, 0);
  EXPECT_NE(str, nullptr);
  EXPECT_EQ(strlen(str), size);
  EXPECT_STREQ(str, "abcd");
  free(str);
}

TEST(Tools, null_length_with_header) {
  char *str = NULL;
  size_t size = 0;
  fill_with_alphabet(&str, size, 1);
  EXPECT_NE(str, nullptr);
  uint64_t content_size = *(uint64_t *)str;
  char *str_content = str + sizeof(uint64_t);
  uint8_t foot = *(uint8_t *)(str + content_size);
  EXPECT_EQ(content_size, size);
  EXPECT_STREQ(str_content, "");
  EXPECT_EQ(foot, 0);
  free(str);
}

TEST(Tools, null_length_without_header) {
  char *str = NULL;
  size_t size = 0;
  fill_with_alphabet(&str, size, 0);
  EXPECT_NE(str, nullptr);
  EXPECT_EQ(strlen(str), size);
  EXPECT_STREQ(str, "");
  free(str);
}

/*!
 * Cuando a check_iovec se le envia que ha de ejecutarse:
 *   CON HEADER -> quiere decir que se va a simular unos iovecs en los que se ha de generar el
 * header, por lo que generamos un mensaje de entrada SIN header (UNA SALIDA)
 *
 *   SIN HEADER -> quiere decir que se va a simular unos iovecs en los que no se han de generar
 * header, por lo que generamos un mensaje CON header (UNA ENTRADA).
 */
static void check_iovec(size_t size, uint8_t h) {
  // mensaje de entrada
  void *msg = NULL;
  fill_with_alphabet((char **)&msg, size, !h);

  // full_size, content_size y amount
  uint64_t full_size = size + (h ? sizeof(uint64_t) + 1 : 0);
  uint64_t content_size = size;
  size_t amount = full_size / CHUNK_SZ;
  amount = amount + (full_size % CHUNK_SZ ? 1 : 0);

  // iovec y chd_ptr
  struct iovec *iovecs = (struct iovec *)malloc(amount * sizeof(struct iovec));
  void **chd_ptr = (void **)malloc(amount * sizeof(void *));

  char *msg_ptr = (char *)msg;
  char iov_str[CHUNK_SZ + 1];
  memset(iov_str, 0, CHUNK_SZ + 1);
  char orig_str[CHUNK_SZ + 1];
  memset(orig_str, 0, CHUNK_SZ + 1);

  int res = 0;
  for (size_t i = 0; i < amount; ++i) {
    // alojamos los iovecs reservando el tamaño del mensaje completo
    res = allocate_iovec(&iovecs[i], amount, i, full_size, chd_ptr);
    EXPECT_EQ(res, SUCCESS_CODE);

    // IOV_LEN esperado
    //   - n -> CHUNK_SZ
    //   - ultimo -> full_size % CHUNK_SZ (+1 si es 0)
    uint64_t exp_iov_len = ((i + 1) == amount ? full_size % CHUNK_SZ : CHUNK_SZ);
    exp_iov_len = (exp_iov_len == 0 ? CHUNK_SZ : exp_iov_len);
    EXPECT_EQ(exp_iov_len, iovecs[i].iov_len);

    // Inicializamos iovec
    res = initialize_iovec(&iovecs[i], amount, i, msg, content_size, h);
    EXPECT_EQ(res, SUCCESS_CODE);

    // Si es el primero verificar el tamaño del contenido
    if (i == 0) {
      EXPECT_EQ(*((uint64_t *)iovecs[i].iov_base), content_size);
    }

    // Descargar el contenido el iovec, tanto con header como sin el la salida es la misma
    //   i = 0 -> se desplaza 8 bytes -> `len` - 8 | `offset` = 8
    //   n -> `len` | `offset` = 0
    //   i = amount -> se quita el foot -> `len` - 1
    size_t str_len =
        iovecs[i].iov_len - (i == 0 ? sizeof(uint64_t) : 0) - ((i + 1) == amount ? 1 : 0);
    strncpy(iov_str, (char *)iovecs[i].iov_base + (i == 0 ? sizeof(uint64_t) : 0), str_len);
    strncpy(orig_str, msg_ptr, str_len);
    iov_str[str_len] = 0;
    orig_str[str_len] = 0;
    EXPECT_EQ(strncmp(iov_str, orig_str, str_len), 0);
    msg_ptr += iovecs[i].iov_len - (i == 0 ? sizeof(uint64_t) : 0);
  }
  free(iovecs);
  for (size_t i = 0; i < amount; ++i) {
    free(chd_ptr[i]);
  }
  free(chd_ptr);
  free(msg);
}

TEST(LowSaurion, initialize_correct_with_header) {
  const char *message = "Hola, Mundo!";
  size_t content_size = strlen(message);
  size_t msg_size = content_size + (sizeof(uint64_t) + 1);
  struct iovec *iovec = (struct iovec *)malloc(sizeof(struct iovec));
  void **chd_ptr = (void **)malloc(sizeof(void *));
  int res = allocate_iovec(&iovec[0], 1, 0, msg_size, chd_ptr);
  EXPECT_EQ(res, SUCCESS_CODE);
  EXPECT_EQ(iovec[0].iov_len, msg_size);
  res = initialize_iovec(&iovec[0], 1, 0, message, content_size, 1);
  EXPECT_EQ(res, SUCCESS_CODE);
  uint64_t size = *(uint64_t *)iovec[0].iov_base;
  EXPECT_EQ(size, content_size);
  EXPECT_EQ(strncmp((char *)iovec[0].iov_base + sizeof(uint64_t), message, content_size), 0);
  EXPECT_EQ(*((char *)iovec[0].iov_base + sizeof(uint64_t) + content_size), 0);
}

TEST(LowSaurion, creates_iovecs_correctly) {
  EXPECT_EQ(1, 1);
  check_iovec(CHUNK_SZ / 2, 1);
  check_iovec(CHUNK_SZ + 53, 1);
  check_iovec(CHUNK_SZ, 1);
  check_iovec(CHUNK_SZ - sizeof(uint64_t) - 1, 1);
  check_iovec(CHUNK_SZ - sizeof(uint64_t), 1);
  check_iovec(0, 1);
  check_iovec(10 * CHUNK_SZ - sizeof(uint64_t), 1);
  for (int i = 0; i < 10; ++i) {
    srand(time(0));
    int chunks = rand() % 10;
    int extra = rand() % CHUNK_SZ;
    check_iovec(chunks * CHUNK_SZ + extra, 1);
  }
}

TEST(LowSaurion, tries_alloc_null_iovec) {
  struct iovec *iovec_null = NULL;
  int res = allocate_iovec(iovec_null, 0, 0, 0, NULL);
  EXPECT_EQ(res, ERROR_CODE);
}

TEST(LowSaurion, tries_init_null_iovec) {
  struct iovec *iovec_null = NULL;
  int res = initialize_iovec(iovec_null, 0, 0, NULL, 0, 1);
  EXPECT_EQ(res, ERROR_CODE);
}

TEST(LowSaurion, create_iovec_for_null_msg) {
  check_iovec(0, 1);
  check_iovec(0, 0);
}

TEST(LowSaurion, set_request_first_creation_and_reset) {
  struct request *req = NULL;
  struct Node *list = NULL;
  size_t size = 2.5 * CHUNK_SZ;
  char *msg = NULL;
  fill_with_alphabet(&msg, size, 0);
  int res = set_request(&req, &list, size, msg, 1);
  EXPECT_EQ(req->prev, nullptr);
  EXPECT_EQ(req->prev_size, 0UL);
  EXPECT_EQ(req->prev_remain, 0UL);
  EXPECT_EQ(req->next_iov, 0UL);
  EXPECT_EQ(req->next_offset, 0UL);
  EXPECT_EQ(req->iovec_count, 3UL);
  EXPECT_EQ(res, SUCCESS_CODE);
  req->client_socket = 123;
  req->event_type = 456;
  res = set_request(&req, &list, size, msg, 1);
  EXPECT_EQ(res, SUCCESS_CODE);
  EXPECT_EQ(req->client_socket, 123);
  EXPECT_EQ(req->event_type, 456);
  free(msg);
}

TEST(LowSaurion, test_free_request) {
  struct request *req = NULL;
  struct Node *list = NULL;
  size_t size = 2.5 * CHUNK_SZ;
  char *msg = NULL;
  fill_with_alphabet(&msg, size, 0);
  int res = set_request(&req, &list, size, msg, 1);
  EXPECT_EQ(req->prev, nullptr);
  EXPECT_EQ(req->prev_size, 0UL);
  EXPECT_EQ(req->prev_remain, 0UL);
  EXPECT_EQ(req->next_iov, 0UL);
  EXPECT_EQ(req->next_offset, 0UL);
  EXPECT_EQ(req->iovec_count, 3UL);
  EXPECT_EQ(res, SUCCESS_CODE);
  free_request(req, NULL, 0);
  free(msg);
}

TEST(LowSaurion, EmptyRequest) {
  // Caso en el que req->iovec_count == 0
  struct request req = {};
  req.iovec_count = 0;
  void *dest = nullptr;
  size_t len = 0;

  int res = read_chunk(&dest, &len, &req);

  EXPECT_EQ(res, ERROR_CODE);
  EXPECT_EQ(dest, nullptr);
  EXPECT_EQ(len, 0u);
}

TEST(LowSaurion, SingleMessageComplete) {
  // Caso en el que hay un mensaje completo en un solo iovec
  const char *message = "Hola, Mundo!";
  size_t msg_size = strlen(message);

  // Crear el buffer que incluye el tamaño del mensaje seguido del mensaje
  struct request *req = NULL;
  struct Node *list = NULL;
  int res = set_request(&req, &list, msg_size, message, 1);
  EXPECT_EQ(res, SUCCESS_CODE);

  void *dest = nullptr;
  size_t len = 0;

  res = read_chunk(&dest, &len, req);

  EXPECT_EQ(res, SUCCESS_CODE);
  EXPECT_EQ(len, msg_size);
  EXPECT_STREQ((char *)dest, message);

  // Limpieza
  free_request(req, NULL, 0);
  free(dest);
}

TEST(LowSaurion, MessageSpanningMultipleIovecs) {
  // Caso en el que un mensaje se divide en múltiples iovecs
  char *message = NULL;
  fill_with_alphabet(&message, 1.5 * CHUNK_SZ, 0);
  size_t msg_size = strlen(message);

  // Crear el buffer que incluye el tamaño del mensaje seguido del mensaje
  struct request *req = NULL;
  struct Node *list = NULL;
  int res = set_request(&req, &list, msg_size, message, 1);
  EXPECT_EQ(res, SUCCESS_CODE);
  EXPECT_EQ(req->prev_size, 0UL);
  EXPECT_EQ(req->prev_remain, 0UL);
  EXPECT_EQ(req->next_iov, 0UL);
  EXPECT_EQ(req->next_offset, 0UL);
  EXPECT_EQ(req->iovec_count, 2UL);
  EXPECT_EQ(*(size_t *)req->iov[0].iov_base, 1.5 * CHUNK_SZ);

  void *dest = nullptr;
  size_t len = 0;

  res = read_chunk(&dest, &len, req);

  EXPECT_EQ(res, SUCCESS_CODE);
  ASSERT_NE(dest, nullptr);
  EXPECT_EQ(len, msg_size);

  // Limpieza
  free_request(req, NULL, 0);
  free(dest);
}

TEST(LowSaurion, PreviousUnfinishedMessage) {
  // Caso en el que un mensaje se divide en múltiples iovecs
  char *message = NULL;
  fill_with_alphabet(&message, 2.5 * CHUNK_SZ, 0);
  size_t msg_size = strlen(message);

  // Crear el buffer que incluye el tamaño del mensaje seguido del mensaje
  struct request *req = NULL;
  struct Node *list = NULL;
  int res = set_request(&req, &list, msg_size, message, 1);
  EXPECT_EQ(res, SUCCESS_CODE);
  EXPECT_EQ(req->prev_size, 0UL);
  EXPECT_EQ(req->prev_remain, 0UL);
  EXPECT_EQ(req->next_iov, 0UL);
  EXPECT_EQ(req->next_offset, 0UL);
  EXPECT_EQ(req->iovec_count, 3UL);
  EXPECT_EQ(*(size_t *)req->iov[0].iov_base, 2.5 * CHUNK_SZ);

  req->iovec_count = 1;

  void *dest = nullptr;
  size_t len = 0;

  res = read_chunk(&dest, &len, req);

  EXPECT_EQ(res, SUCCESS_CODE);
  ASSERT_EQ(dest, nullptr);
  EXPECT_EQ(len, 0UL);
  EXPECT_NE(req->prev, nullptr);
  EXPECT_EQ(req->prev_size, msg_size);
  size_t readed = CHUNK_SZ - sizeof(size_t);
  EXPECT_EQ(req->prev_remain, msg_size - readed);

  res = set_request(&req, &list, req->prev_remain, message + readed, 0);
  EXPECT_EQ(res, SUCCESS_CODE);

  req->iovec_count = 1;

  res = read_chunk(&dest, &len, req);
  EXPECT_EQ(res, SUCCESS_CODE);
  ASSERT_EQ(dest, nullptr);
  EXPECT_EQ(len, 0UL);
  EXPECT_NE(req->prev, nullptr);
  EXPECT_EQ(req->prev_size, msg_size);
  readed = 2 * CHUNK_SZ - sizeof(size_t);
  EXPECT_EQ(req->prev_remain, msg_size - readed);

  res = set_request(&req, &list, req->prev_remain, message + readed, 0);
  EXPECT_EQ(res, SUCCESS_CODE);

  res = read_chunk(&dest, &len, req);
  EXPECT_EQ(res, SUCCESS_CODE);
  ASSERT_NE(dest, nullptr);
  EXPECT_EQ(len, msg_size);

  // Limpieza
  free_request(req, NULL, 0);
  free(dest);
}
