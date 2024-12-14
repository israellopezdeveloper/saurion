#include "config.h"             // for CHUNK_SZ, SUCCESS_CODE, ERROR_CODE
#include "linked_list.h"        // for list_free
#include "low_saurion_secret.h" // for request, set_request, read_chunk
#include "gtest/gtest.h"        // for Message, TestPartResult, Test (ptr o...

#include <arpa/inet.h> // for htonl, ntohl
#include <cstdlib>     // for free, uint64_t
#include <cstring>     // for strlen, strncmp, memcpy, memset, str...
#include <random>      // for uniform_int_distribution, random_device

uint64_t
htonll (uint64_t value)
{
  int num = 42;
  if (*(char *)&num == 42)
    {
      uint32_t high_part = htonl ((uint32_t)(value >> 32));
      uint32_t low_part = htonl ((uint32_t)(value & 0xFFFFFFFFLL));
      return ((uint64_t)low_part << 32) | high_part;
    }
  else
    {
      return value;
    }
}

uint64_t
ntohll (uint64_t value)
{
  int num = 42;
  if (*(char *)&num == 42)
    {
      uint32_t high_part = ntohl ((uint32_t)(value >> 32));
      uint32_t low_part = ntohl ((uint32_t)(value & 0xFFFFFFFFLL));
      return ((uint64_t)low_part << 32) | high_part;
    }
  else
    {
      return value;
    }
}

std::unique_ptr<char[]>
fill_with_alphabet (uint64_t length, uint8_t h)
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
      const uint64_t content_size = ntohll (*(const uint64_t *)str);
      const char *const str_content = str + sizeof (uint64_t);
      const uint8_t foot = *(const uint8_t *)(str + content_size);
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
  auto msg = fill_with_alphabet (size, !h);

  uint64_t full_size = size + (h ? sizeof (uint64_t) + 1 : 0);
  uint64_t content_size = size;
  uint64_t amount = full_size / CHUNK_SZ;
  amount = amount + (full_size % CHUNK_SZ ? 1 : 0);

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
      res = allocate_iovec (&iovecs[i], amount, i, full_size, chd_ptr);
      EXPECT_EQ (res, SUCCESS_CODE);

      uint64_t exp_iov_len
          = ((i + 1) == amount ? full_size % CHUNK_SZ : CHUNK_SZ);
      exp_iov_len = (exp_iov_len == 0 ? CHUNK_SZ : exp_iov_len);
      EXPECT_EQ (exp_iov_len, iovecs[i].iov_len);

      res = initialize_iovec (&iovecs[i], amount, i, msg.get (), content_size,
                              h);
      EXPECT_EQ (res, SUCCESS_CODE);

      if (i == 0)
        {
          uint64_t exp_size = *((uint64_t *)iovecs[i].iov_base);
          exp_size = ntohll (exp_size);
          EXPECT_EQ (exp_size, content_size);
        }

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

static void
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

static void
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

static void
check_msg (const struct request *const r, uint64_t &o, const uint64_t s,
           const bool ok)
{
  uint64_t exp_size = *(uint64_t *)((uint8_t *)r->iov[0].iov_base + o);
  exp_size = ntohll (exp_size);
  EXPECT_EQ (exp_size, s);
  uint64_t numiovs = (s + sizeof (uint64_t)) / CHUNK_SZ;
  uint64_t offset = (s + sizeof (uint64_t)) % CHUNK_SZ;
  if (numiovs)
    {
      o = offset;
    }
  else
    {
      o += sizeof (uint64_t) + s;
    }
  uint8_t foot = *(uint8_t *)((char *)r->iov[numiovs].iov_base + o);
  if (ok)
    {
      EXPECT_EQ (foot, 0);
    }
  o += 1;
}

static std::vector<std::unique_ptr<char[]> >
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

  for (uint64_t i = 0; i < sizes.size (); ++i)
    {
      total_size += sizes[i] + wrapper + adjustments[i];
    }

  auto msgs = std::make_unique<char[]> (total_size);

  offset = 0;
  std::vector<std::unique_ptr<char[]> > message_list;

  for (uint64_t i = 0; i < sizes.size (); ++i)
    {
      auto msg = fill_with_alphabet (sizes[i], 1);
      std::memcpy (msgs.get () + offset, msg.get (),
                   sizes[i] + wrapper + adjustments[i]);
      offset += sizes[i] + wrapper + adjustments[i];
      message_list.push_back (std::move (msg));
    }

  auto sum_msg = std::make_unique<char[]> (total_size);
  uint64_t sum_offset = 0;

  for (uint64_t i = 0; i < sizes.size (); ++i)
    {
      uint64_t msg_size = sizes[i];
      std::memcpy (sum_msg.get () + sum_offset, message_list[i].get (),
                   msg_size + wrapper + adjustments[i]);
      sum_offset += msg_size + wrapper + adjustments[i];
    }

  message_list.push_back (std::move (sum_msg));

  return message_list;
}

static void
check_set_msgs (const std::vector<uint64_t> &s, const std::vector<int> &a,
                const int r)
{
  if ((r > 0) || (r * -1 > (int)s[s.size () - 1]))
    {
      throw std::invalid_argument ("Invalid remain");
    }
  auto msgs_vector = generate_messages (s, a);

  struct request *req = nullptr;
  struct Node *list = nullptr;

  auto total_size = std::accumulate (s.begin (), s.end (), 0) + s.size () * 9
                    + std::accumulate (a.begin (), a.end (), 0);
  int res = set_request (&req, &list, total_size,
                         msgs_vector[s.size ()].get (), 0);
  EXPECT_EQ (res, SUCCESS_CODE);
  uint64_t iovs = std::ceil ((float)total_size / CHUNK_SZ);
  check_request (res, req, iovs);
  req->iov[req->iovec_count - 1].iov_len += r;
  uint64_t offset = 0;
  for (uint i = 0; i < s.size (); ++i)
    {
      check_msg (req, offset, s[i], a[i] == 0);
      offset += a[i];
    }

  void *dest = nullptr;
  uint64_t len = 0;
  for (uint i = 0; i < s.size () - (r != 0 ? 1 : 0); ++i)
    {
      res = read_chunk (&dest, &len, req);
      if (a[i] == 0)
        {
          check_chunk (req, res, msgs_vector[i].get (), dest, len, s[i],
                       (i < (s.size () - 1)));
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
  if (r != 0)
    {
      res = read_chunk (&dest, &len, req);
      EXPECT_EQ (res, SUCCESS_CODE);
      ASSERT_EQ (dest, nullptr);
      EXPECT_EQ (len, 0UL);
      EXPECT_NE (req->prev, nullptr);
      EXPECT_EQ (req->prev_size, s[s.size () - 1]);
      uint64_t readed = s[s.size () - 1] + r + 1;
      EXPECT_EQ (req->prev_remain, s[s.size () - 1] - readed);
      free (req->prev);
    }

  list_free (&list);
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
  struct request req;
  req.iovec_count = 0;
  void *dest = nullptr;
  uint64_t len = 0;

  int res = read_chunk (&dest, &len, &req);

  EXPECT_EQ (res, ERROR_CODE);
  EXPECT_EQ (dest, nullptr);
  EXPECT_EQ (len, 0u);
}

TEST (unit_saurion, SingleMessageComplete)
{
  const char *message = "Hola, Mundo!";
  uint64_t msg_size = strlen (message);

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

  list_free (&list);
  free (dest);
}

TEST (unit_saurion, MessageSpanningMultipleIovecs)
{
  std::vector<uint64_t> sizes = { (uint64_t)(1.5 * CHUNK_SZ) };
  std::vector<int> adjustments = { 0 };
  check_set_msgs (sizes, adjustments, 0);
}

void
check_read (const uint64_t len, const uint64_t msg_size, const uint64_t readed,
            const int res, const struct request *req, const void *const dest)
{
  EXPECT_EQ (res, SUCCESS_CODE);
  ASSERT_EQ (dest, nullptr);
  EXPECT_EQ (len, 0UL);
  EXPECT_NE (req->prev, nullptr);
  EXPECT_EQ (req->prev_size, msg_size);
  EXPECT_EQ (req->prev_remain, msg_size - readed);
}

TEST (unit_saurion, PreviousUnfinishedMessage)
{
  auto message = fill_with_alphabet (2.5 * CHUNK_SZ, 0);
  uint64_t msg_size = strlen (message.get ());

  struct request *req = nullptr;
  struct Node *list = nullptr;
  int res = set_request (&req, &list, msg_size, message.get (), 1);
  check_request (res, req, 3UL);
  uint64_t size = *(uint64_t *)req->iov[0].iov_base;
  size = ntohll (size);
  EXPECT_EQ (size, 2.5 * CHUNK_SZ);

  req->iovec_count = 1;

  void *dest = nullptr;
  uint64_t len = 0;

  res = read_chunk (&dest, &len, req);
  uint64_t readed = CHUNK_SZ - sizeof (uint64_t);

  check_read (len, msg_size, readed, res, req, dest);

  res = set_request (&req, &list, req->prev_remain, message.get () + readed,
                     0);
  EXPECT_EQ (res, SUCCESS_CODE);

  req->iovec_count = 1;

  res = read_chunk (&dest, &len, req);
  readed = 2 * CHUNK_SZ - sizeof (uint64_t);
  check_read (len, msg_size, readed, res, req, dest);

  res = set_request (&req, &list, req->prev_remain, message.get () + readed,
                     0);
  EXPECT_EQ (res, SUCCESS_CODE);

  res = read_chunk (&dest, &len, req);
  EXPECT_EQ (res, SUCCESS_CODE);
  ASSERT_NE (dest, nullptr);
  EXPECT_EQ (len, msg_size);

  list_free (&list);
  free (dest);
}

TEST (unit_saurion, MultipleMessagesInOneIovec)
{
  std::vector<uint64_t> sizes = { 3, 4, 5 };
  std::vector<int> adjustments = { 0, 0, 0 };
  check_set_msgs (sizes, adjustments, 0);
}

TEST (unit_saurion, MultipleMessagesInOneIovecLastIncomplete)
{
  std::vector<uint64_t> sizes = { 3, 4, 5 };
  std::vector<int> adjustments = { 0, 0, 0 };
  check_set_msgs (sizes, adjustments, -3);
}

TEST (unit_saurion, MultipleMessagesInOneIovecSecondMalformed)
{
  std::vector<uint64_t> sizes = { 10, 40, 50 };
  std::vector<int> adjustments = { 0, -10, 0 };
  check_set_msgs (sizes, adjustments, 0);
}

TEST (unit_saurion, MultipleMessagesInOneIovecSecondAndThirdMalformed)
{
  std::vector<uint64_t> sizes = { 10, 40, 50 };
  std::vector<int> adjustments = { 0, -10, -5 };
  check_set_msgs (sizes, adjustments, 0);
}
