#include "low_saurion.h"

#include <bits/types/struct_iovec.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "gtest/gtest.h"
#include "low_saurion_secret.h"

void fill_with_alphabet(char *str, size_t length, uint8_t h) {
  const char *alphabet = "abcdefghijklmnopqrstuvwxyz";  // El alfabeto
  int alphabet_len = 26;                                // Longitud del alfabeto

  if (h) {
    for (size_t i = 0; i < length; i++) {
      str[i] = alphabet[i % alphabet_len];  // Ciclar a través del alfabeto
    }
  } else {
    size_t wrapper = sizeof(uint64_t) + sizeof(char);
    size_t content_size = (length - wrapper);
    if (length >= wrapper) {
      memset(str, content_size, sizeof(uint64_t));
      int pos = sizeof(uint64_t);
      for (size_t i = 0; i < content_size; i++) {
        str[pos] = alphabet[i % alphabet_len];  // Ciclar a través del alfabeto
        pos++;
      }

      str[length - 1] = '\0';  // Asegurarse de que la cadena esté terminada con '\0'
    }
  }
}

static void check_iovec(size_t content_size, uint8_t h) {
  void *msg = (content_size > 0 ? malloc(content_size) : NULL);
  fill_with_alphabet((char *)msg, content_size, h);
  size_t full_size = content_size + (h ? sizeof(uint64_t) + 1 : 0);
  content_size -= (h ? 0 : sizeof(uint64_t) + 1);
  size_t amount = full_size / CHUNK_SZ;
  amount = amount + (full_size % CHUNK_SZ ? 1 : 0);
  int res = 0;
  struct iovec *iovecs = (struct iovec *)malloc(amount * sizeof(struct iovec));
  void **chd_ptr = (void **)malloc(amount * sizeof(void *));
  char *msg_ptr = (char *)msg;
  char iov_str[CHUNK_SZ + 1];
  memset(iov_str, 0, CHUNK_SZ + 1);
  char orig_str[CHUNK_SZ + 1];
  memset(orig_str, 0, CHUNK_SZ + 1);
  for (size_t i = 0; i < amount; ++i) {
    res = allocate_iovec(&iovecs[i], amount, i, full_size, chd_ptr);
    EXPECT_EQ(res, SUCCESS_CODE);
    size_t exp_iov_len =
        ((i + 1) < amount ? CHUNK_SZ : (content_size + (sizeof(uint64_t) + 1)) % CHUNK_SZ);
    exp_iov_len = (exp_iov_len == 0 ? CHUNK_SZ : exp_iov_len);
    EXPECT_EQ(exp_iov_len, iovecs[i].iov_len);
    res = initialize_iovec(&iovecs[i], amount, i, msg, content_size, 1);
    EXPECT_EQ(res, SUCCESS_CODE);
    if (i == 0) {
      EXPECT_EQ(*((size_t *)iovecs[i].iov_base), content_size);
    }
    size_t str_len =
        iovecs[i].iov_len - (h && i == 0 ? sizeof(uint64_t) : 0) - ((i + 1) == amount ? 1 : 0);
    strncpy(iov_str, (char *)iovecs[i].iov_base + (h && i == 0 ? sizeof(uint64_t) : 0), str_len);
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
  char *msg = (char *)malloc(size);
  fill_with_alphabet(msg, size, 0);
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
}

TEST(LowSaurion, test_free_request) {
  struct request *req = NULL;
  struct Node *list = NULL;
  size_t size = 2.5 * CHUNK_SZ;
  char *msg = (char *)malloc(size);
  fill_with_alphabet(msg, size, 0);
  int res = set_request(&req, &list, size, msg, 1);
  EXPECT_EQ(req->prev, nullptr);
  EXPECT_EQ(req->prev_size, 0UL);
  EXPECT_EQ(req->prev_remain, 0UL);
  EXPECT_EQ(req->next_iov, 0UL);
  EXPECT_EQ(req->next_offset, 0UL);
  EXPECT_EQ(req->iovec_count, 3UL);
  EXPECT_EQ(res, SUCCESS_CODE);
  free_request(req, NULL, 0);
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

// TEST(LowSaurion, MessageSpanningMultipleIovecs) {
//   // Caso en el que un mensaje se divide en múltiples iovecs
//   char *message = (char *)malloc(2.5 * CHUNK_SZ + 1);
//   fill_with_alphabet(message, 2.5 * CHUNK_SZ + 1, 0);
//   size_t msg_size = strlen(message);
//
//   // Crear el buffer que incluye el tamaño del mensaje seguido del mensaje
//   struct request *req = NULL;
//   struct Node *list = NULL;
//   int res = set_request(&req, &list, msg_size, message, 1);
//   EXPECT_EQ(res, SUCCESS_CODE);
//   EXPECT_EQ(req->prev_size, 0UL);
//   EXPECT_EQ(req->prev_remain, 0UL);
//   EXPECT_EQ(req->next_iov, 0UL);
//   EXPECT_EQ(req->next_offset, 0UL);
//   EXPECT_EQ(req->iovec_count, 3UL);
//   EXPECT_EQ(*(size_t *)req->iov[0].iov_base, 2.5 * CHUNK_SZ);
//
//   void *dest = nullptr;
//   size_t len = 0;
//
//   res = read_chunk(&dest, &len, req);
//
//   EXPECT_EQ(res, SUCCESS_CODE);
//   ASSERT_NE(dest, nullptr);
//   EXPECT_EQ(len, msg_size);
//   EXPECT_STREQ((char *)dest, message);
//
//   // Limpieza
//   free_request(req, NULL, 0);
//   free(message);
//   free(dest);
// }
//
// TEST(LowSaurion, IncompleteMessage) {
//   // Caso en el que el mensaje es incompleto y se almacenará en req->prev
//   char *message = (char *)malloc(2.5 * CHUNK_SZ + 1);
//   fill_with_alphabet(message, 2.5 * CHUNK_SZ + 1, 0);
//   size_t msg_size = strlen(message);
//
//   // Crear el buffer que incluye el tamaño del mensaje seguido del mensaje
//   struct request *req = NULL;
//   struct Node *list = NULL;
//   int res = set_request(&req, &list, msg_size, message, 1);
//   req->iovec_count = 1;
//   EXPECT_EQ(res, SUCCESS_CODE);
//   EXPECT_EQ(req->prev_size, 0UL);
//   EXPECT_EQ(req->prev_remain, 0UL);
//   EXPECT_EQ(req->next_iov, 0UL);
//   EXPECT_EQ(req->next_offset, 0UL);
//   EXPECT_EQ(req->iovec_count, 1UL);
//   EXPECT_EQ(*(size_t *)req->iov[0].iov_base, 2.5 * CHUNK_SZ);
//
//   void *dest = nullptr;
//   size_t len = 0;
//
//   res = read_chunk(&dest, &len, req);
//
//   EXPECT_EQ(res, SUCCESS_CODE);
//   // Como el mensaje es incompleto, dest debería ser nullptr y len debería ser 0
//   EXPECT_EQ(dest, nullptr);
//   EXPECT_EQ(len, 0u);
//
//   // Verificar que req->prev tiene los datos parciales
//   ASSERT_NE(req->prev, nullptr);
//   EXPECT_EQ(req->prev_size, msg_size);
//   EXPECT_EQ(req->prev_remain, msg_size - (CHUNK_SZ - sizeof(uint64_t)));
//
//   // Simular la llegada del resto del mensaje en un nuevo iovec
//   free(message);
//   message = NULL;
//   message = (char *)malloc(req->prev_remain + 1);
//   memset(message, 'A', req->prev_remain);
//   message[req->prev_remain] = 0;
//   msg_size = strlen(message);
//   res = set_request(&req, &list, msg_size, message, 0);
//   EXPECT_EQ(res, SUCCESS_CODE);
//   EXPECT_NE(req->prev, nullptr);
//   EXPECT_EQ(req->prev_remain, msg_size);
//
//   res = read_chunk(&dest, &len, req);
//
//   EXPECT_EQ(res, SUCCESS_CODE);
//   ASSERT_NE(dest, nullptr);
//   // EXPECT_EQ(len, msg_size);
//   // EXPECT_STREQ((char *)dest, message);
//
//   // // Limpieza
//   // free(dest);
//   // free(buffer);
//   // free(buffer2);
//   // free(req.prev);
// }
