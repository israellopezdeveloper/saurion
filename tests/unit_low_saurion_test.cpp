#include <bits/types/struct_iovec.h>
#include <gtest/gtest.h>
#include <memory>
#include <netinet/in.h>
#include <random>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "config.h"
#include "linked_list.h"
#include "low_saurion_secret.h"

uint64_t
htonll (uint64_t value)
{
  int num = 42;
  if (*(char *)&num == 42)
    { // Little endian check
      uint32_t high_part = htonl ((uint32_t)(value >> 32));
      uint32_t low_part = htonl ((uint32_t)(value & 0xFFFFFFFFLL));
      return ((uint64_t)low_part << 32) | high_part;
    }
  else
    { // Already big endian
      return value;
    }
}

uint64_t
ntohll (uint64_t value)
{
  int num = 42;
  if (*(char *)&num == 42)
    { // Little endian check
      uint32_t high_part = ntohl ((uint32_t)(value >> 32));
      uint32_t low_part = ntohl ((uint32_t)(value & 0xFFFFFFFFLL));
      return ((uint64_t)low_part << 32) | high_part;
    }
  else
    { // Already big endian
      return value;
    }
}

std::unique_ptr<char[]>
fill_with_alphabet (size_t length, uint8_t h)
{
  const char *alphabet = "abcdefghijklmnopqrstuvwxyz";
  int alphabet_len = 26;

  uint64_t wrapper = sizeof (uint64_t) + sizeof (char);
  std::unique_ptr<char[]> result;
  if (h)
    {
      uint64_t msg_size = length + wrapper;
      result = std::make_unique<char[]> (msg_size);
      *(uint64_t *)(result.get ()) = htonll (length);
      int pos = sizeof (uint64_t);
      for (uint64_t i = 0; i < length; i++)
        {
          result[pos] = alphabet[i % alphabet_len];
          pos++;
        }
      result[msg_size - 1] = 0;
    }
  else
    {
      result = std::make_unique<char[]> (length + 1);
      int pos = 0;
      for (uint64_t i = 0; i < length; i++)
        {
          result[pos] = alphabet[i % alphabet_len];
          pos++;
        }
      result[length] = 0;
    }
  return result;
}

void
check_alpha_msg (const uint64_t s, const char *const str, const char *exp,
                 const bool h)
{
  if (h)
    {
      EXPECT_NE (str, nullptr);
      uint64_t content_size = ntohll (*(uint64_t *)str);
      const char *const str_content = str + sizeof (uint64_t);
      uint8_t foot = *(uint8_t *)(str + content_size);
      EXPECT_EQ (content_size, s);
      EXPECT_STREQ (str_content, exp);
      EXPECT_EQ (foot, 0);
    }
  else
    {
      EXPECT_NE (str, nullptr);
      EXPECT_EQ (strlen (str), s);
      EXPECT_STREQ (str, exp);
    }
}

TEST (Tools, alphabet_with_header)
{
  uint64_t size = 4;
  auto str = fill_with_alphabet (size, 1);
  check_alpha_msg (size, str.get (), "abcd", true);
}

TEST (Tools, alphabet_without_header)
{
  uint64_t size = 4;
  auto str = fill_with_alphabet (size, 0);
  check_alpha_msg (size, str.get (), "abcd", false);
}

TEST (Tools, null_length_with_header)
{
  uint64_t size = 0;
  auto str = fill_with_alphabet (size, 1);
  check_alpha_msg (size, str.get (), "", true);
}

TEST (Tools, null_length_without_header)
{
  uint64_t size = 0;
  auto str = fill_with_alphabet (size, 0);
  check_alpha_msg (size, str.get (), "", false);
}

/*!
 * Cuando a check_iovec se le envia que ha de ejecutarse:
 *   CON HEADER -> quiere decir que se va a simular unos iovecs en los que se
 * ha de generar el header, por lo que generamos un mensaje de entrada SIN
 * header (UNA SALIDA)
 *
 *   SIN HEADER -> quiere decir que se va a simular unos iovecs en los que no
 * se han de generar header, por lo que generamos un mensaje CON header (UNA
 * ENTRADA).
 */
static void
check_iovec (uint64_t size, uint8_t h)
{
  // mensaje de entrada
  auto msg = fill_with_alphabet (size, !h);

  // full_size, content_size y amount
  uint64_t full_size = size + (h ? sizeof (uint64_t) + 1 : 0);
  uint64_t content_size = size;
  uint64_t amount = full_size / CHUNK_SZ;
  amount = amount + (full_size % CHUNK_SZ ? 1 : 0);

  // iovec y chd_ptr
  auto *iovecs = new struct iovec[amount];
  auto **chd_ptr = new void *[amount];

  auto *msg_ptr = msg.get ();
  char iov_str[CHUNK_SZ + 1];
  memset (iov_str, 0, CHUNK_SZ + 1);
  char orig_str[CHUNK_SZ + 1];
  memset (orig_str, 0, CHUNK_SZ + 1);

  int res = 0;
  for (uint64_t i = 0; i < amount; ++i)
    {
      // alojamos los iovecs reservando el tamaño del mensaje completo
      res = allocate_iovec (&iovecs[i], amount, i, full_size, chd_ptr);
      EXPECT_EQ (res, SUCCESS_CODE);

      // IOV_LEN esperado
      //   - n -> CHUNK_SZ
      //   - ultimo -> full_size % CHUNK_SZ (+1 si es 0)
      uint64_t exp_iov_len
          = ((i + 1) == amount ? full_size % CHUNK_SZ : CHUNK_SZ);
      exp_iov_len = (exp_iov_len == 0 ? CHUNK_SZ : exp_iov_len);
      EXPECT_EQ (exp_iov_len, iovecs[i].iov_len);

      // Inicializamos iovec
      res = initialize_iovec (&iovecs[i], amount, i, msg.get (), content_size,
                              h);
      EXPECT_EQ (res, SUCCESS_CODE);

      // Si es el primero verificar el tamaño del contenido
      if (i == 0)
        {
          uint64_t exp_size = *((uint64_t *)iovecs[i].iov_base);
          exp_size = ntohll (exp_size);
          EXPECT_EQ (exp_size, content_size);
        }

      // Descargar el contenido el iovec, tanto con header como sin el la
      // salida es la misma
      //   i = 0 -> se desplaza 8 bytes -> `len` - 8 | `offset` = 8
      //   n -> `len` | `offset` = 0
      //   i = amount -> se quita el foot -> `len` - 1
      uint64_t str_len = iovecs[i].iov_len - (i == 0 ? sizeof (uint64_t) : 0)
                         - ((i + 1) == amount ? 1 : 0);
      strncpy (iov_str,
               (char *)iovecs[i].iov_base + (i == 0 ? sizeof (uint64_t) : 0),
               str_len);
      strncpy (orig_str, msg_ptr, str_len);
      iov_str[str_len] = 0;
      orig_str[str_len] = 0;
      EXPECT_EQ (strncmp (iov_str, orig_str, str_len), 0);
      msg_ptr += iovecs[i].iov_len - (i == 0 ? sizeof (uint64_t) : 0);
    }
  delete[] iovecs;
  for (uint64_t i = 0; i < amount; ++i)
    {
      free (chd_ptr[i]);
    }
  delete[] chd_ptr;
}

TEST (unit_saurion, initialize_correct_with_header)
{
  const char *message = "Hola, Mundo!";
  uint64_t content_size = strlen (message);
  uint64_t msg_size = content_size + (sizeof (uint64_t) + 1);
  auto *iovec = new struct iovec;
  auto **chd_ptr = new void *;
  int res = allocate_iovec (&iovec[0], 1, 0, msg_size, chd_ptr);
  EXPECT_EQ (res, SUCCESS_CODE);
  EXPECT_EQ (iovec[0].iov_len, msg_size);
  res = initialize_iovec (&iovec[0], 1, 0, message, content_size, 1);
  EXPECT_EQ (res, SUCCESS_CODE);
  uint64_t size = *(uint64_t *)iovec[0].iov_base;
  size = ntohll (size);
  EXPECT_EQ (size, content_size);
  EXPECT_EQ (strncmp ((char *)iovec[0].iov_base + sizeof (uint64_t), message,
                      content_size),
             0);
  EXPECT_EQ (*((char *)iovec[0].iov_base + sizeof (uint64_t) + content_size),
             0);
  free (iovec[0].iov_base);
  delete iovec;
  delete chd_ptr;
}

TEST (unit_saurion, creates_iovecs_correctly)
{
  EXPECT_EQ (1, 1);
  std::random_device rd;
  std::mt19937 gen (rd ());
  std::uniform_int_distribution dis (0, 10);
  std::uniform_int_distribution dis2 (0, CHUNK_SZ);
  check_iovec (CHUNK_SZ / 2, 1);
  check_iovec (CHUNK_SZ + 53, 1);
  check_iovec (CHUNK_SZ, 1);
  check_iovec (CHUNK_SZ - sizeof (uint64_t) - 1, 1);
  check_iovec (CHUNK_SZ - sizeof (uint64_t), 1);
  check_iovec (0, 1);
  check_iovec (10 * CHUNK_SZ - sizeof (uint64_t), 1);
  for (int i = 0; i < 10; ++i)
    {
      ;
      int chunks = dis (gen);
      int extra = dis2 (gen);
      check_iovec (chunks * CHUNK_SZ + extra, 1);
    }
}

TEST (unit_saurion, tries_alloc_null_iovec)
{
  struct iovec *iovec_null = nullptr;
  int res = allocate_iovec (iovec_null, 0, 0, 0, nullptr);
  EXPECT_EQ (res, ERROR_CODE);
}

TEST (unit_saurion, tries_init_null_iovec)
{
  struct iovec *iovec_null = nullptr;
  int res = initialize_iovec (iovec_null, 0, 0, nullptr, 0, 1);
  EXPECT_EQ (res, ERROR_CODE);
}

TEST (unit_saurion, create_iovec_for_null_msg)
{
  check_iovec (0, 1);
  check_iovec (0, 0);
}

void
check_request (const int res, const struct request *const r,
               const uint64_t iovecs)
{
  EXPECT_EQ (res, SUCCESS_CODE);
  EXPECT_EQ (r->prev, nullptr);
  EXPECT_EQ (r->prev_size, 0UL);
  EXPECT_EQ (r->prev_remain, 0UL);
  EXPECT_EQ (r->next_iov, 0UL);
  EXPECT_EQ (r->next_offset, 0UL);
  EXPECT_EQ (r->iovec_count, iovecs);
}

TEST (unit_saurion, set_request_first_creation_and_reset)
{
  struct request *req = nullptr;
  struct Node *list = nullptr;
  uint64_t size = 2.5 * CHUNK_SZ;
  auto msg = fill_with_alphabet (size, 0);
  int res = set_request (&req, &list, size, msg.get (), 1);
  check_request (res, req, 3UL);
  req->client_socket = 123;
  req->event_type = 456;
  res = set_request (&req, &list, size, msg.get (), 1);
  check_request (res, req, 3UL);
  EXPECT_EQ (req->client_socket, 123);
  EXPECT_EQ (req->event_type, 456);
  list_free (&list);
}

TEST (unit_saurion, test_free_request)
{
  struct request *req = nullptr;
  struct Node *list = nullptr;
  uint64_t size = 2.5 * CHUNK_SZ;
  auto msg = fill_with_alphabet (size, 0);
  int res = set_request (&req, &list, size, msg.get (), 1);
  check_request (res, req, 3UL);
  list_free (&list);
}

TEST (unit_saurion, EmptyRequest)
{
  // Caso en el que req->iovec_count == 0
  struct request req;
  req.iovec_count = 0;
  void *dest = nullptr;
  uint64_t len = 0;

  int res = read_chunk (&dest, &len, &req);

  EXPECT_EQ (res, ERROR_CODE);
  EXPECT_EQ (dest, nullptr);
  EXPECT_EQ (len, 0u);
}

void
check_chunk (const struct request *const r, int res, const char *const m,
             void *const d, const uint64_t l, const uint64_t s, const bool n)
{
  EXPECT_EQ (res, SUCCESS_CODE);
  ASSERT_NE (d, nullptr);
  EXPECT_EQ (l, s);
  EXPECT_EQ (strncmp ((char *)d, m + sizeof (uint64_t), s), 0);
  if (n)
    {
      EXPECT_NE (r->next_offset, 0UL);
    }
  else
    {
      EXPECT_EQ (r->next_offset, 0UL);
    }
  free (d);
}

TEST (unit_saurion, SingleMessageComplete)
{
  // Caso en el que hay un mensaje completo en un solo iovec
  const char *message = "Hola, Mundo!";
  uint64_t msg_size = strlen (message);

  // Crear el buffer que incluye el tamaño del mensaje seguido del mensaje
  struct request *req = nullptr;
  struct Node *list = nullptr;
  int res = set_request (&req, &list, msg_size, message, 1);
  EXPECT_EQ (res, SUCCESS_CODE);

  void *dest = nullptr;
  uint64_t len = 0;

  res = read_chunk (&dest, &len, req);

  EXPECT_EQ (res, SUCCESS_CODE);
  EXPECT_EQ (len, msg_size);
  EXPECT_EQ (strncmp ((char *)dest, message, len), 0);

  // Limpieza
  list_free (&list);
  free (dest);
}

TEST (unit_saurion, MessageSpanningMultipleIovecs)
{
  // Caso en el que un mensaje se divide en múltiples iovecs
  auto message = fill_with_alphabet (1.5 * CHUNK_SZ, 0);
  uint64_t msg_size = strlen (message.get ());

  // Crear el buffer que incluye el tamaño del mensaje seguido del mensaje
  struct request *req = nullptr;
  struct Node *list = nullptr;
  int res = set_request (&req, &list, msg_size, message.get (), 1);
  check_request (res, req, 2UL);
  uint64_t size = *(uint64_t *)req->iov[0].iov_base;
  size = ntohll (size);
  EXPECT_EQ (size, 1.5 * CHUNK_SZ);

  void *dest = nullptr;
  uint64_t len = 0;

  res = read_chunk (&dest, &len, req);

  EXPECT_EQ (res, SUCCESS_CODE);
  ASSERT_NE (dest, nullptr);
  EXPECT_EQ (len, msg_size);

  // Limpieza
  list_free (&list);
  free (dest);
}

TEST (unit_saurion, PreviousUnfinishedMessage)
{
  // Caso en el que un mensaje se divide en múltiples iovecs
  auto message = fill_with_alphabet (2.5 * CHUNK_SZ, 0);
  uint64_t msg_size = strlen (message.get ());

  // Crear el buffer que incluye el tamaño del mensaje seguido del mensaje
  struct request *req = nullptr;
  struct Node *list = nullptr;
  int res = set_request (&req, &list, msg_size, message.get (), 1);
  check_request (res, req, 3UL);
  EXPECT_EQ (res, SUCCESS_CODE);
  EXPECT_EQ (req->prev_size, 0UL);
  EXPECT_EQ (req->prev_remain, 0UL);
  EXPECT_EQ (req->next_iov, 0UL);
  EXPECT_EQ (req->next_offset, 0UL);
  EXPECT_EQ (req->iovec_count, 3UL);
  uint64_t size = *(uint64_t *)req->iov[0].iov_base;
  size = ntohll (size);
  EXPECT_EQ (size, 2.5 * CHUNK_SZ);

  req->iovec_count = 1;

  void *dest = nullptr;
  uint64_t len = 0;

  res = read_chunk (&dest, &len, req);

  EXPECT_EQ (res, SUCCESS_CODE);
  ASSERT_EQ (dest, nullptr);
  EXPECT_EQ (len, 0UL);
  EXPECT_NE (req->prev, nullptr);
  EXPECT_EQ (req->prev_size, msg_size);
  uint64_t readed = CHUNK_SZ - sizeof (size_t);
  EXPECT_EQ (req->prev_remain, msg_size - readed);

  res = set_request (&req, &list, req->prev_remain, message.get () + readed,
                     0);
  EXPECT_EQ (res, SUCCESS_CODE);

  req->iovec_count = 1;

  res = read_chunk (&dest, &len, req);
  EXPECT_EQ (res, SUCCESS_CODE);
  ASSERT_EQ (dest, nullptr);
  EXPECT_EQ (len, 0UL);
  EXPECT_NE (req->prev, nullptr);
  EXPECT_EQ (req->prev_size, msg_size);
  readed = 2 * CHUNK_SZ - sizeof (uint64_t);
  EXPECT_EQ (req->prev_remain, msg_size - readed);

  res = set_request (&req, &list, req->prev_remain, message.get () + readed,
                     0);
  EXPECT_EQ (res, SUCCESS_CODE);

  res = read_chunk (&dest, &len, req);
  EXPECT_EQ (res, SUCCESS_CODE);
  ASSERT_NE (dest, nullptr);
  EXPECT_EQ (len, msg_size);

  // Limpieza
  list_free (&list);
  free (dest);
}

void
check_msg (const struct request *const r, uint64_t &o, const uint64_t s,
           const bool ok)
{
  uint64_t exp_size = *(uint64_t *)((uint8_t *)r->iov[0].iov_base + o);
  exp_size = ntohll (exp_size);
  EXPECT_EQ (exp_size, s);
  o += sizeof (uint64_t) + s;
  exp_size = *(uint64_t *)((char *)r->iov[0].iov_base + o);
  exp_size = ntohll (exp_size);
  if (ok)
    {
      EXPECT_EQ (exp_size, 0UL);
    }
  o += 1;
}

std::vector<std::unique_ptr<char[]> >
generate_messages (const std::vector<uint64_t> &sizes,
                   const std::vector<int> &adjustments)
{
  if (sizes.size () != adjustments.size ())
    {
      throw std::invalid_argument (
          "Sizes and adjustments vectors must have the same length");
    }
  uint64_t offset = 0;
  for (const auto &a : adjustments)
    {
      if ((a > 0) || (a * -1 > (int)sizes[offset]))
        {
          throw std::invalid_argument (
              "Sizes and adjustments vectors must be less or equal to 0 and "
              "smaller than size");
        }
    }

  const uint64_t wrapper = sizeof (uint64_t) + sizeof (uint8_t);
  uint64_t total_size = 0;

  // Calcular el tamaño total ajustado
  for (size_t i = 0; i < sizes.size (); ++i)
    {
      total_size += sizes[i] + wrapper + adjustments[i];
    }

  // Crear un buffer para almacenar todos los mensajes concatenados
  auto msgs = std::make_unique<char[]> (total_size);

  offset = 0;
  std::vector<std::unique_ptr<char[]> > message_list;

  // Procesar cada mensaje
  for (size_t i = 0; i < sizes.size (); ++i)
    {
      auto msg = fill_with_alphabet (sizes[i], 1);
      std::memcpy (msgs.get () + offset, msg.get (),
                   sizes[i] + wrapper + adjustments[i]);
      offset += sizes[i] + wrapper + adjustments[i];
      message_list.push_back (std::move (msg));
    }

  // Concatenar todos los mensajes en un único mensaje
  auto sum_msg = std::make_unique<char[]> (total_size);
  uint64_t sum_offset = 0;

  for (size_t i = 0; i < sizes.size (); ++i)
    {
      uint64_t msg_size = sizes[i];
      std::memcpy (sum_msg.get () + sum_offset, message_list[i].get (),
                   msg_size + wrapper + adjustments[i]);
      sum_offset += msg_size + wrapper + adjustments[i];
    }

  // Añadir el mensaje combinado al final del vector
  message_list.push_back (std::move (sum_msg));

  return message_list;
}

void
check_set_msgs (const std::vector<uint64_t> &sizes,
                const std::vector<int> &adjustments)
{
  auto msgs_vector = generate_messages (sizes, adjustments);

  struct request *req = nullptr;
  struct Node *list = nullptr;

  auto total_size
      = std::accumulate (sizes.begin (), sizes.end (), 0) + sizes.size () * 9
        + std::accumulate (adjustments.begin (), adjustments.end (), 0);
  int res = set_request (&req, &list, total_size,
                         msgs_vector[sizes.size ()].get (), 0);
  check_request (res, req, 1UL);
  uint64_t offset = 0;
  for (uint i = 0; i < sizes.size (); ++i)
    {
      check_msg (req, offset, sizes[i], adjustments[i] == 0);
      offset += adjustments[i];
    }

  void *dest = nullptr;
  uint64_t len = 0;
  for (uint i = 0; i < sizes.size (); ++i)
    {
      res = read_chunk (&dest, &len, req);
      if (adjustments[i] == 0)
        {
          check_chunk (req, res, msgs_vector[i].get (), dest, len, sizes[i],
                       (i < (sizes.size () - 1)));
        }
      else
        {
          EXPECT_EQ (res, ERROR_CODE);
          ASSERT_EQ (dest, nullptr);
          EXPECT_EQ (len, 0UL);
          EXPECT_EQ (req->prev, nullptr);
          EXPECT_EQ (req->prev_remain, 0UL);
          EXPECT_EQ (req->prev_size, 0UL);
          EXPECT_EQ (req->next_offset, 0UL);
          break;
        }
    }

  list_free (&list);
}

TEST (unit_saurion, MultipleMessagesInOneIovec)
{
  std::vector<uint64_t> sizes = { 3, 4, 5 };
  std::vector<int> adjustments = { 0, 0, 0 };
  check_set_msgs (sizes, adjustments);
}

TEST (unit_saurion, MultipleMessagesInOneIovecLastIncomplete)
{
  uint64_t size1 = 3;
  uint64_t size2 = 4;
  uint64_t size3 = 5;
  uint64_t offset = 0;
  auto msg1 = fill_with_alphabet (size1, 1);
  auto msg2 = fill_with_alphabet (size2, 1);
  auto msg3 = fill_with_alphabet (size3, 1);

  const uint64_t wrapper = sizeof (uint64_t) + 1;
  uint64_t total_size = size1 + size2 + size3 + 3 * wrapper;
  auto msgs = std::make_unique<char[]> (total_size);
  std::memcpy (msgs.get (), msg1.get (), size1 + wrapper);
  offset += size1 + wrapper;
  std::memcpy (msgs.get () + offset, msg2.get (), size2 + wrapper);
  offset += size2 + wrapper;
  std::memcpy (msgs.get () + offset, msg3.get (), size3 + wrapper);

  struct request *req = nullptr;
  struct Node *list = nullptr;

  int res = set_request (&req, &list, total_size, msgs.get (), 0);
  check_request (res, req, 1UL);
  req->iov[0].iov_len -= 3;
  offset = 0;
  check_msg (req, offset, size1, true);
  check_msg (req, offset, size2, true);
  check_msg (req, offset, size3, true);

  void *dest = nullptr;
  uint64_t len = 0;

  res = read_chunk (&dest, &len, req);
  check_chunk (req, res, msg1.get (), dest, len, size1, true);

  res = read_chunk (&dest, &len, req);
  check_chunk (req, res, msg2.get (), dest, len, size2, true);

  res = read_chunk (&dest, &len, req);

  EXPECT_EQ (res, SUCCESS_CODE);
  ASSERT_EQ (dest, nullptr);
  EXPECT_EQ (len, 0UL);
  EXPECT_NE (req->prev, nullptr);
  EXPECT_EQ (req->prev_size, size3);
  uint64_t readed = size3 - 2;
  EXPECT_EQ (req->prev_remain, size3 - readed);
  free (req->prev);
  list_free (&list);
}

TEST (unit_saurion, MultipleMessagesInOneIovecSecondMalformed)
{
  std::vector<uint64_t> sizes = { 10, 40, 50 };
  std::vector<int> adjustments = { 0, -10, 0 };
  check_set_msgs (sizes, adjustments);
}

TEST (unit_saurion, MultipleMessagesInOneIovecSecondAndThirdMalformed)
{
  std::vector<uint64_t> sizes = { 10, 40, 50 };
  std::vector<int> adjustments = { 0, -10, -5 };
  check_set_msgs (sizes, adjustments);
}
