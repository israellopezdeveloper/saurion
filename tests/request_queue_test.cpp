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
protected:
  struct request_queue queue;

  void
  SetUp () override
  {
    ASSERT_EQ (init_queue (&queue), SUCCESS_CODE);
  }

  void
  TearDown () override
  {
    destroy_queue (&queue);
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
  struct request *req = (struct request *)malloc (sizeof (struct request));
  req->id = 1;
  ASSERT_EQ (enqueue (&queue, req), SUCCESS_CODE);
  ASSERT_EQ (queue.front->req, req);
  ASSERT_EQ (queue.rear->req, req);
}

TEST_F (RequestQueueTest, EnqueueMultipleRequests)
{
  std::vector<struct request *> requests
      = { (struct request *)malloc (sizeof (struct request)),
          (struct request *)malloc (sizeof (struct request)),
          (struct request *)malloc (sizeof (struct request)),
          (struct request *)malloc (sizeof (struct request)) };

  int counter = 0;
  for (auto *req : requests)
    {
      ++counter;
      req->id = counter;
      ASSERT_EQ (enqueue (&queue, req), SUCCESS_CODE);
    }

  struct queue_node *node = queue.front;
  for (size_t i = 0; i < requests.size (); ++i)
    {
      ASSERT_EQ (node->req, requests[i]);
      node = node->next;
    }
  ASSERT_EQ (queue.rear->req, requests.back ());
}

TEST_F (RequestQueueTest, DequeueSingleRequest)
{
  struct request *req = (struct request *)malloc (sizeof (struct request));
  req->id = 1;
  ASSERT_EQ (enqueue (&queue, req), SUCCESS_CODE);

  struct request *dequeued_req = dequeue (&queue);
  ASSERT_EQ (dequeued_req, req);
  ASSERT_EQ (queue.front, nullptr);
  ASSERT_EQ (queue.rear, nullptr);
}

TEST_F (RequestQueueTest, DequeueMultipleRequests)
{
  std::vector<struct request *> requests
      = { (struct request *)malloc (sizeof (struct request)),
          (struct request *)malloc (sizeof (struct request)),
          (struct request *)malloc (sizeof (struct request)),
          (struct request *)malloc (sizeof (struct request)) };

  for (auto *req : requests)
    {
      ASSERT_EQ (enqueue (&queue, req), SUCCESS_CODE);
    }

  for (auto *req : requests)
    {
      struct request *dequeued_req = dequeue (&queue);
      ASSERT_EQ (dequeued_req, req);
    }

  ASSERT_EQ (queue.front, nullptr);
  ASSERT_EQ (queue.rear, nullptr);
}

TEST_F (RequestQueueTest, DequeueEmptyQueue)
{
  std::atomic<bool> dequeued (false);
  std::thread t ([&] () {
    ASSERT_NE (dequeue (&queue), nullptr);
    dequeued.store (true);
  });

  struct request *req = (struct request *)malloc (sizeof (struct request));
  ASSERT_EQ (enqueue (&queue, req), SUCCESS_CODE);

  t.join ();
  ASSERT_TRUE (dequeued.load ());
}

TEST_F (RequestQueueTest, DestroyQueueWithItems)
{
  std::vector<struct request *> requests;
  for (int i = 0; i < 5; ++i)
    {
      struct request *req = (struct request *)malloc (sizeof (struct request));
      req->id = i;
      requests.push_back (req);
      ASSERT_EQ (enqueue (&queue, req), SUCCESS_CODE);
    }

  destroy_queue (&queue);

  ASSERT_EQ (queue.front, nullptr);
  ASSERT_EQ (queue.rear, nullptr);
}
