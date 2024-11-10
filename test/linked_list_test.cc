extern "C" {
#include "linked_list.h"
}
#include <gtest/gtest.h>

#include "linked_list.h"
#include <gtest/gtest.h>

class LinkedListTest : public ::testing::Test {
protected:
  List list;

  void SetUp() override { list = create_list(); }

  void TearDown() override { destroy_list(list); }
};

TEST_F(LinkedListTest, CreateList) {
  EXPECT_NE(list, nullptr);
  EXPECT_EQ(get_list_length(list), 0);
}

TEST_F(LinkedListTest, LPush) {
  int length;
  EXPECT_EQ(lpush(list, "test", &length), 0);
  EXPECT_EQ(length, 1);
  EXPECT_EQ(get_list_length(list), 1);
}

TEST_F(LinkedListTest, RPush) {
  int length;
  EXPECT_EQ(rpush(list, "test", &length), 0);
  EXPECT_EQ(length, 1);
  EXPECT_EQ(get_list_length(list), 1);
}

TEST_F(LinkedListTest, LpushAndRPush) {
  int length;
  EXPECT_EQ(lpush(list, "first", &length), 0);
  EXPECT_EQ(rpush(list, "second", &length), 0);
  EXPECT_EQ(length, 2);
  EXPECT_EQ(get_list_length(list), 2);
}