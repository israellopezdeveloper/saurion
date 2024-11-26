#include "threadpool.h" // for struct threadpool

#include <pthread.h>
#include <sys/stat.h>

#include <ctime>

#include "gtest/gtest.h" // for Test, Message, TestInfo (ptr only), TEST

class struct_threadpool : public ::testing::Test
{
public:
  struct threadpool *pool;
  struct shared_variable
  {
    int x = 0;
    pthread_mutex_t mtx;
  };
  static struct shared_variable sv;

protected:
  void
  SetUp () override
  {
    sv.x = 0;
    pthread_mutex_init (&sv.mtx, nullptr);
    pool = threadpool_create (6);
    threadpool_init (pool);
  }

  void
  TearDown () override
  {
    threadpool_destroy (pool);
    sv.x = 0;
    pthread_mutex_destroy (&sv.mtx);
  }

  static void
  thread_function (void *ptr)
  {
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000;
    nanosleep (&ts, nullptr);
    auto *sv_ptr = static_cast<struct shared_variable *> (ptr);
    pthread_mutex_lock (&sv_ptr->mtx);
    ++sv_ptr->x;
    pthread_mutex_unlock (&sv_ptr->mtx);
  }
};

struct_threadpool::shared_variable struct_threadpool::sv{};

TEST_F (struct_threadpool, DefaultConstructor)
{
  EXPECT_EQ (threadpool_empty (pool), true);
}

TEST_F (struct_threadpool, AddTask)
{
  threadpool_add (pool, struct_threadpool::thread_function,
                  &struct_threadpool::sv);
  threadpool_stop (pool);
  pthread_mutex_lock (&struct_threadpool::sv.mtx);
  EXPECT_EQ (struct_threadpool::sv.x, 1);
  pthread_mutex_unlock (&struct_threadpool::sv.mtx);
}

TEST_F (struct_threadpool, AddMultipleTasks)
{
  const int N = 100;
  for (int i = 0; i < N; ++i)
    {
      threadpool_add (pool, struct_threadpool::thread_function,
                      &struct_threadpool::sv);
    }
  threadpool_stop (pool);
  pthread_mutex_lock (&struct_threadpool::sv.mtx);
  EXPECT_EQ (struct_threadpool::sv.x, N);
  pthread_mutex_unlock (&struct_threadpool::sv.mtx);
}

TEST_F (struct_threadpool, StopThreadPool)
{
  threadpool_stop (pool);
  EXPECT_TRUE (threadpool_empty (pool));
}

TEST_F (struct_threadpool, ZeroThreads)
{
  int x = 0;
  threadpool_add (
      pool,
      [] (void *arg) {
        auto *ptr = static_cast<int *> (arg);
        (*ptr)++;
      },
      &x);
  threadpool_stop (pool);
  EXPECT_EQ (x, 1);
}
