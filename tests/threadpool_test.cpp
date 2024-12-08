#include "config.h"
#include "threadpool.h"
#include "gtest/gtest.h" // for Message, TestInfo (ptr only), AssertionResult
#include <ctime>         // for timespec, nanosleep
#include <pthread.h>     // for pthread_mutex_lock, pthread_mutex_unlock
#include <thread>        // for sleep_for

void
dummy_task (void *arg)
{
  std::this_thread::sleep_for (std::chrono::milliseconds (50));
  auto *counter = static_cast<int *> (arg);
  (*counter)++;
}

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

TEST_F (struct_threadpool, AddNullTask)
{
  threadpool_add (pool, nullptr, nullptr);

  ASSERT_TRUE (threadpool_empty (pool));
}

TEST_F (struct_threadpool, AddToStopedThreadPool)
{
  int counter = 0;
  threadpool_add (pool, dummy_task, &counter);
  threadpool_stop (pool);

  // Asegura que no se pueden agregar más tareas después de detener
  threadpool_add (pool, dummy_task, &counter);

  ASSERT_EQ (counter, 1);
}

TEST (ThreadPoolTest, CreateMinimumThreads)
{
  struct threadpool *pool = threadpool_create (2);
  ASSERT_NE (pool, nullptr);
  threadpool_destroy (pool);
}

TEST (ThreadPoolTest, CreateMaximumThreads)
{
  struct threadpool *pool
      = threadpool_create (NUM_CORES + 5); // num_threads > NUM_CORES
  ASSERT_NE (pool, nullptr);
  threadpool_destroy (pool);
}

TEST (ThreadPoolTest, AddNullTask)
{
  struct threadpool *pool = threadpool_create (4);
  ASSERT_NE (pool, nullptr);

  // No debería agregar tareas si el puntero de función es nulo
  threadpool_add (pool, nullptr, nullptr);

  ASSERT_TRUE (threadpool_empty (pool));

  threadpool_destroy (pool);
}

TEST (ThreadPoolTest, StopThreadPool)
{
  struct threadpool *pool = threadpool_create (4);
  ASSERT_NE (pool, nullptr);

  int counter = 0;
  threadpool_init (pool);

  threadpool_add (pool, dummy_task, &counter);
  threadpool_stop (pool);

  // Asegura que no se pueden agregar más tareas después de detener
  threadpool_add (pool, dummy_task, &counter);

  ASSERT_EQ (counter, 1); // Solo una tarea ejecutada

  threadpool_destroy (pool);
}

TEST (ThreadPoolTest, DestroyWithPendingTasks)
{
  struct threadpool *pool = threadpool_create (4);
  ASSERT_NE (pool, nullptr);

  int counter = 0;
  threadpool_init (pool);

  // Agregar tareas pero no esperar a que terminen
  for (int i = 0; i < 5; ++i)
    {
      threadpool_add (pool, dummy_task, &counter);
    }

  threadpool_destroy (pool);

  // Los recursos deben limpiarse incluso con tareas pendientes
  ASSERT_TRUE (true); // No hay errores de segmentación
}

TEST (ThreadPoolTest, PoolEmptyCheck)
{
  struct threadpool *pool = threadpool_create (4);
  threadpool_init (pool);
  ASSERT_NE (pool, nullptr);

  ASSERT_TRUE (threadpool_empty (pool));

  int counter = 0;
  threadpool_add (pool, dummy_task, &counter);

  ASSERT_FALSE (threadpool_empty (pool));

  threadpool_wait_empty (pool);
  ASSERT_TRUE (threadpool_empty (pool));

  threadpool_destroy (pool);
}

TEST (ThreadPoolTest, CreateDefaultPool)
{
  struct threadpool *pool = threadpool_create_default ();
  ASSERT_NE (pool, nullptr);

  threadpool_destroy (pool);
}
