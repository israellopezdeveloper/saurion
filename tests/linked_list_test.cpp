#include "linked_list.h"

#include <sys/uio.h>  // for iovec

#include "gtest/gtest.h"  // for AssertionResult, Message, TestInfo (ptr only)

#define N_ITEMS 100

class LinkedListTest : public ::testing::Test {
 public:
  struct Node *list = nullptr;

 protected:
  void SetUp() override {
    list_free(&list);
    list = nullptr;
  }

  void TearDown() override {
    list_free(&list);
    list = nullptr;
  }

  void *insert_simple_item(const char *const str) {
    char *ptr = (char *)malloc(strlen(str) + 1);
    strcpy(ptr, str);
    list_insert(&list, ptr, 0, NULL);
    return ptr;
  }

  void *insert_complex_item(const char *const str) {
    char *ptr = (char *)malloc(strlen(str) + 1);
    strcpy(ptr, str);
    size_t amount = 4;
    struct iovec **children = (struct iovec **)malloc(sizeof(struct iovec *) * amount);
    void **children_ptr = (void **)malloc(sizeof(void *) * amount);
    for (size_t i = 0; i < amount; ++i) {
      children[i] = (struct iovec *)malloc(sizeof(struct iovec));
      children[i]->iov_len = 7;
      children[i]->iov_base = (char *)malloc(children[i]->iov_len);
      strcpy((char *)children[i]->iov_base, "Hola h");
      children_ptr[i] = children[i]->iov_base;
    }

    list_insert(&list, ptr, amount, children_ptr);

    for (size_t i = 0; i < amount; ++i) {
      free(children[i]);
    }
    free(children);
    free(children_ptr);
    return ptr;
  }
};

TEST_F(LinkedListTest, insertSimpleItems) {
  for (int i = 0; i < N_ITEMS; ++i) {
    insert_simple_item("item 1");
  }
  EXPECT_TRUE(true);
}

TEST_F(LinkedListTest, insertComplexItems) {
  for (int i = 0; i < N_ITEMS; ++i) {
    insert_complex_item("item 1");
  }
  EXPECT_TRUE(true);
}

TEST_F(LinkedListTest, insertAndDeleteSimpleItems) {
  for (int i = 0; i < N_ITEMS; ++i) {
    insert_simple_item("item 1");
  }
  void *ptr = insert_simple_item("item to delete");
  for (int i = 0; i < N_ITEMS; ++i) {
    insert_simple_item("item 1");
  }
  list_delete_node(&list, ptr);
  EXPECT_TRUE(true);
}

TEST_F(LinkedListTest, insertAndDeleteComplexItems) {
  for (int i = 0; i < N_ITEMS; ++i) {
    insert_complex_item("item 1");
  }
  void *ptr = insert_complex_item("item to delete");
  for (int i = 0; i < N_ITEMS; ++i) {
    insert_complex_item("item 1");
  }
  list_delete_node(&list, ptr);
  EXPECT_TRUE(true);
}

TEST_F(LinkedListTest, tryDeleteNotExistentItem) {
  for (int i = 0; i < N_ITEMS; ++i) {
    insert_complex_item("item 1");
  }
  void *ptr = new char;
  for (int i = 0; i < N_ITEMS; ++i) {
    insert_complex_item("item 1");
  }
  list_delete_node(&list, ptr);
  delete static_cast<char *>(ptr);
  EXPECT_TRUE(true);
}

TEST_F(LinkedListTest, insertAndDeleteHeadItem) {
  void *ptr = insert_complex_item("item to delete");
  for (int i = 0; i < N_ITEMS; ++i) {
    insert_complex_item("item 1");
  }
  list_delete_node(&list, ptr);
  EXPECT_TRUE(true);
}
