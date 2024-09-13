#include "low_saurion.h"

#include <bits/types/struct_iovec.h>

#include <cstddef>
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

    if (length > 0) {
      str[length - 1] = '\0';  // Asegurarse de que la cadena esté terminada con '\0'
    }
  } else {
    size_t wrapper = sizeof(size_t) + sizeof(char);
    size_t content_size = (length - wrapper);
    if (length >= wrapper) {
      memset(str, content_size, sizeof(size_t));
      int pos = sizeof(size_t);
      for (size_t i = 0; i < content_size; i++) {
        str[pos] = alphabet[i % alphabet_len];  // Ciclar a través del alfabeto
        pos++;
      }

      str[length - 1] = '\0';  // Asegurarse de que la cadena esté terminada con '\0'
    }
  }
}

static void check_iovec(size_t content_size, uint8_t h) {
  void *msg = malloc(content_size);
  fill_with_alphabet((char *)msg, content_size, h);
  size_t full_size = content_size + (h ? sizeof(size_t) + 1 : 0);
  content_size -= (h ? 0 : sizeof(size_t) + 1);
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
        ((i + 1) < amount ? CHUNK_SZ : (content_size + (sizeof(size_t) + 1)) % CHUNK_SZ);
    exp_iov_len = (exp_iov_len == 0 ? CHUNK_SZ : exp_iov_len);
    EXPECT_EQ(exp_iov_len, iovecs[i].iov_len);
    res = initialize_iovec(&iovecs[i], amount, i, msg, content_size, 1);
    EXPECT_EQ(res, SUCCESS_CODE);
    if (i == 0) {
      EXPECT_EQ(*((size_t *)iovecs[i].iov_base), content_size);
    }
    size_t str_len =
        iovecs[i].iov_len - (h && i == 0 ? sizeof(size_t) : 0) - ((i + 1) == amount ? 1 : 0);
    strncpy(iov_str, (char *)iovecs[i].iov_base + (h && i == 0 ? sizeof(size_t) : 0), str_len);
    strncpy(orig_str, msg_ptr, str_len);
    iov_str[str_len] = 0;
    orig_str[str_len] = 0;
    EXPECT_EQ(strncmp(iov_str, orig_str, str_len), 0);
    msg_ptr += iovecs[i].iov_len - (i == 0 ? sizeof(size_t) : 0);
  }
  free(iovecs);
  for (size_t i = 0; i < amount; ++i) {
    free(chd_ptr[i]);
  }
  free(chd_ptr);
  free(msg);
}

/*TEST(LowSaurion, creates_iovecs_correctly) { check_iovec(4, 1); }*/
TEST(LowSaurion, creates_iovecs_correctly) {
  EXPECT_EQ(1, 1);
  check_iovec(CHUNK_SZ / 2, 1);
  check_iovec(CHUNK_SZ + 53, 1);
  check_iovec(CHUNK_SZ, 1);
  check_iovec(CHUNK_SZ - sizeof(size_t) - 1, 1);
  check_iovec(CHUNK_SZ - sizeof(size_t), 1);
  check_iovec(0, 1);
  check_iovec(10 * CHUNK_SZ - sizeof(size_t), 1);
  for (int i = 0; i < 10; ++i) {
    srand(time(0));
    int chunks = rand() % 10;
    int extra = rand() % CHUNK_SZ;
    check_iovec(chunks * CHUNK_SZ + extra, 1);
  }
}
