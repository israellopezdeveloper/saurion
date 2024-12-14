#include "linked_list.h"
#include "gtest/gtest.h"

#include <atomic>
#include <sys/uio.h> // for struct iovec
#include <thread>    // for jthread

constexpr int N_ITEMS = 100;

class LinkedListTest : public ::testing::Test
{
public:
  struct Node *list = nullptr;

protected:
  void
  TearDown () override
  {
    list_free (&list);
    list = nullptr;
  }

  char *
  insert_simple_item (const char *const str)
  {
    auto *ptr = new char[strlen (str) + 1];
    strcpy (ptr, str);
    if (!list_insert (&list, ptr, 0, nullptr))
      {
        return nullptr;
      }
    return ptr;
  }

  char *
  insert_complex_item (const char *const str)
  {
    auto *ptr = new char[strlen (str) + 1];
    strcpy (ptr, str);
    uint64_t amount = 4;
    auto **children = new struct iovec *[amount];
    auto **children_ptr = new void *[amount];
    for (uint64_t i = 0; i < amount; ++i)
      {
        children[i] = new struct iovec;
        children[i]->iov_len = 7;
        children[i]->iov_base = new char[children[i]->iov_len];
        strcpy ((char *)children[i]->iov_base, "Hola h");
        children_ptr[i] = children[i]->iov_base;
      }

    if (!list_insert (&list, ptr, amount, children_ptr))
      {
        return nullptr;
      }

    for (uint64_t i = 0; i < amount; ++i)
      {
        delete children[i];
      }
    delete[] children;
    delete[] children_ptr;
    return ptr;
  }
};

TEST_F (LinkedListTest, insertSimpleItems)
{
  for (int i = 0; i < N_ITEMS; ++i)
    {
      insert_simple_item ("item 1");
    }
  EXPECT_TRUE (true);
}

TEST_F (LinkedListTest, insertComplexItems)
{
  for (int i = 0; i < N_ITEMS; ++i)
    {
      insert_complex_item ("item 1");
    }
  EXPECT_TRUE (true);
}

TEST_F (LinkedListTest, insertAndDeleteSimpleItems)
{
  for (int i = 0; i < N_ITEMS; ++i)
    {
      insert_simple_item ("item 1");
    }
  void *ptr = insert_simple_item ("item to delete");
  for (int i = 0; i < N_ITEMS; ++i)
    {
      insert_simple_item ("item 1");
    }
  list_delete_node (&list, ptr);
  EXPECT_TRUE (true);
}

TEST_F (LinkedListTest, insertAndDeleteComplexItems)
{
  for (int i = 0; i < N_ITEMS; ++i)
    {
      insert_complex_item ("item 1");
    }
  void *ptr = insert_complex_item ("item to delete");
  for (int i = 0; i < N_ITEMS; ++i)
    {
      insert_complex_item ("item 1");
    }
  list_delete_node (&list, ptr);
  EXPECT_TRUE (true);
}

TEST_F (LinkedListTest, tryDeleteNotExistentItem)
{
  for (int i = 0; i < N_ITEMS; ++i)
    {
      insert_complex_item ("item 1");
    }
  void *ptr = new char;
  for (int i = 0; i < N_ITEMS; ++i)
    {
      insert_complex_item ("item 1");
    }
  list_delete_node (&list, ptr);
  delete static_cast<char *> (ptr);
  EXPECT_TRUE (true);
}

TEST_F (LinkedListTest, insertAndDeleteHeadItem)
{
  void *ptr = insert_complex_item ("item to delete");
  for (int i = 0; i < N_ITEMS; ++i)
    {
      insert_complex_item ("item 1");
    }
  list_delete_node (&list, ptr);
  EXPECT_TRUE (true);
}

constexpr int N_THREADS = 100;
constexpr int ITEMS_PER_THREAD = 100;

class LinkedListConcurrencyTest : public LinkedListTest
{
public:
  void
  concurrent_insert_simple ()
  {
    for (int i = 0; i < ITEMS_PER_THREAD; ++i)
      {
        insert_simple_item ("concurrent item");
      }
  }

  void
  concurrent_insert_complex ()
  {
    for (int i = 0; i < ITEMS_PER_THREAD; ++i)
      {
        insert_complex_item ("concurrent item");
      }
  }

  void
  concurrent_insert_and_delete_simple ()
  {
    for (int i = 0; i < ITEMS_PER_THREAD; ++i)
      {
        void *ptr = insert_simple_item ("to delete");
        list_delete_node (&list, ptr);
      }
  }

  void
  concurrent_insert_and_delete_complex ()
  {
    for (int i = 0; i < ITEMS_PER_THREAD; ++i)
      {
        void *ptr = insert_complex_item ("to delete");
        list_delete_node (&list, ptr);
      }
  }
};

TEST_F (LinkedListConcurrencyTest, ConcurrentInsertSimpleItems)
{
  std::vector<std::jthread> threads;
  for (int i = 0; i < N_THREADS; ++i)
    {
      threads.emplace_back (
          &LinkedListConcurrencyTest::concurrent_insert_simple, this);
    }

  for (auto &thread : threads)
    {
      thread.join ();
    }

  EXPECT_TRUE (true);
}

TEST_F (LinkedListConcurrencyTest, ConcurrentInsertComplexItems)
{
  std::vector<std::jthread> threads;
  for (int i = 0; i < N_THREADS; ++i)
    {
      threads.emplace_back (
          &LinkedListConcurrencyTest::concurrent_insert_complex, this);
    }

  for (auto &thread : threads)
    {
      thread.join ();
    }

  EXPECT_TRUE (true);
}

TEST_F (LinkedListConcurrencyTest, ConcurrentInsertAndDeleteSimpleItems)
{
  std::vector<std::jthread> threads;
  for (int i = 0; i < N_THREADS; ++i)
    {
      threads.emplace_back (
          &LinkedListConcurrencyTest::concurrent_insert_and_delete_simple,
          this);
    }

  for (auto &thread : threads)
    {
      thread.join ();
    }

  EXPECT_TRUE (true);
}

TEST_F (LinkedListConcurrencyTest, ConcurrentInsertAndDeleteComplexItems)
{
  std::vector<std::jthread> threads;
  for (int i = 0; i < N_THREADS; ++i)
    {
      threads.emplace_back (
          &LinkedListConcurrencyTest::concurrent_insert_and_delete_complex,
          this);
    }

  for (auto &thread : threads)
    {
      thread.join ();
    }

  EXPECT_TRUE (true);
}

TEST_F (LinkedListConcurrencyTest, ConcurrentInsertAndDeleteDifferentItems)
{
  std::atomic deletions (0);
  std::vector<std::jthread> threads;

  auto insert_task = [this] () {
    for (int i = 0; i < ITEMS_PER_THREAD; ++i)
      {
        insert_complex_item ("inserted item");
      }
  };

  auto delete_task = [this, &deletions] () {
    for (int i = 0; i < ITEMS_PER_THREAD; ++i)
      {
        void *ptr = insert_complex_item ("to delete");
        list_delete_node (&list, ptr);
        deletions++;
      }
  };

  for (int i = 0; i < N_THREADS / 2; ++i)
    {
      threads.emplace_back (insert_task);
    }

  for (int i = 0; i < N_THREADS / 2; ++i)
    {
      threads.emplace_back (delete_task);
    }

  for (auto &thread : threads)
    {
      thread.join ();
    }

  EXPECT_EQ (deletions, (N_THREADS / 2) * ITEMS_PER_THREAD);
}
