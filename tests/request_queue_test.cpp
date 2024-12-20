#include "config.h"
#include "request_queue.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "gtest/gtest.h"

// Mock request structure for testing
struct request
{
  int id;
};

class RequestQueueTest : public ::testing::Test
{
private:
  struct request_queue queue;

protected:
  void
  SetUp () override
  {
    ASSERT_EQ (init_queue (&queue), SUCCESS_CODE);
  }

  void
  TearDown () override
  {
    destroy_queue ();
  }

public:
  int
  enqueue (struct request *req)
  {
    return ::enqueue (&queue, req);
  }

  struct request *
  dequeue ()
  {
    return ::dequeue (&queue);
  }

  struct queue_node *
  front ()
  {
    return queue.front;
  }

  struct queue_node *
  rear ()
  {
    return queue.rear;
  }

  void
  destroy_queue ()
  {
    ::destroy_queue (&queue);
  }
};

TEST_F (RequestQueueTest, InitQueueSuccess)
{
  struct request_queue test_queue;
  ASSERT_EQ (init_queue (&test_queue), SUCCESS_CODE);
  ASSERT_EQ (test_queue.front, nullptr);
  ASSERT_EQ (test_queue.rear, nullptr);
  pthread_mutex_destroy (&test_queue.lock);
  pthread_cond_destroy (&test_queue.cond);
}

TEST_F (RequestQueueTest, EnqueueSingleRequest)
{
  auto *req = new struct request ({ 1 });
  ASSERT_EQ (enqueue (req), SUCCESS_CODE);
  ASSERT_EQ (front ()->req, req);
  ASSERT_EQ (rear ()->req, req);
}

TEST_F (RequestQueueTest, EnqueueMultipleRequests)
{
  std::vector<struct request *> requests
      = { new struct request ({ 1 }), new struct request ({ 2 }),
          new struct request ({ 3 }), new struct request ({ 4 }) };

  for (auto *req : requests)
    {
      ASSERT_EQ (enqueue (req), SUCCESS_CODE);
    }

  struct queue_node *node = front ();
  for (auto *req : requests)
    {
      ASSERT_EQ (node->req, req);
      node = node->next;
    }
  ASSERT_EQ (rear ()->req, requests.back ());
}

TEST_F (RequestQueueTest, DequeueSingleRequest)
{
  auto *req = new struct request ({ 1 });
  ASSERT_EQ (enqueue (req), SUCCESS_CODE);

  struct request *dequeued_req = dequeue ();
  ASSERT_EQ (dequeued_req, req);
  ASSERT_EQ (front (), nullptr);
  ASSERT_EQ (rear (), nullptr);
}

TEST_F (RequestQueueTest, DequeueMultipleRequests)
{
  std::vector<struct request *> requests
      = { new struct request ({ 1 }), new struct request ({ 2 }),
          new struct request ({ 3 }), new struct request ({ 4 }) };

  for (auto *req : requests)
    {
      ASSERT_EQ (enqueue (req), SUCCESS_CODE);
    }

  for (auto *req : requests)
    {
      struct request *dequeued_req = dequeue ();
      ASSERT_EQ (dequeued_req, req);
    }

  ASSERT_EQ (front (), nullptr);
  ASSERT_EQ (rear (), nullptr);
}

TEST_F (RequestQueueTest, DequeueEmptyQueue)
{
  std::atomic dequeued (false);
  std::thread t ([&] () {
    ASSERT_NE (dequeue (), nullptr);
    dequeued.store (true);
  });

  auto *req = new struct request ({ 1 });
  ASSERT_EQ (enqueue (req), SUCCESS_CODE);

  t.join ();
  ASSERT_TRUE (dequeued.load ());
}

TEST_F (RequestQueueTest, DestroyQueueWithItems)
{
  std::vector<struct request *> requests;
  for (int i = 0; i < 5; ++i)
    {
      auto *req = new struct request ({ 1 });
      requests.push_back (req);
      ASSERT_EQ (enqueue (req), SUCCESS_CODE);
    }

  destroy_queue ();

  ASSERT_EQ (front (), nullptr);
  ASSERT_EQ (rear (), nullptr);
}
