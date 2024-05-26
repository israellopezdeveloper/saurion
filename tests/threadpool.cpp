// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "../epoll/threadpool/threadpool.hpp"

#include <gtest/gtest.h>

// Test Task constructor
TEST(TaskTest, Constructor) {
  void (*nfn)(void*) = nullptr;
  void* arg = nullptr;
  Task task(nfn, arg);
  EXPECT_EQ(task.function, nfn);
  EXPECT_EQ(task.argument, arg);
}

// Test AsyncQueue push and front methods
TEST(AsyncQueueTest, PushAndFront) {
  ThreadPool::AsyncQueue queue(1);
  void (*nfn)(void*) = nullptr;
  void* arg = nullptr;
  Task task(nfn, arg);
  queue.push(std::move(task));
  Task* frontTask = queue.front();
  EXPECT_EQ(frontTask->function, nfn);
  EXPECT_EQ(frontTask->argument, arg);
  delete frontTask;
}

// Test AsyncMultiQueue new_queue and remove_queue methods
TEST(AsyncMultiQueueTest, NewAndRemoveQueue) {
  ThreadPool::AsyncMultiQueue multiQueue;
  multiQueue.new_queue(1, 1);
  EXPECT_FALSE(multiQueue.empty());
  multiQueue.remove_queue(1);
  EXPECT_TRUE(multiQueue.empty());
}

// Test ThreadPool add and empty methods
TEST(ThreadPoolTest, AddAndEmpty) {
  ThreadPool pool;
  void (*nfn)(void*) = nullptr;
  void* arg = nullptr;
  pool.add(nfn, arg);
  EXPECT_FALSE(pool.empty());
}
