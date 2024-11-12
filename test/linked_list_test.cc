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

TEST_F(LinkedListTest, LrangeFullRange) {
  int length;
  int range_length;

  EXPECT_EQ(rpush(list, "item1", &length), 0);
  EXPECT_EQ(rpush(list, "item2", &length), 0);
  EXPECT_EQ(rpush(list, "item3", &length), 0);

  char **range = lrange(list, 0, 2, &range_length);

  ASSERT_NE(range, nullptr);
  EXPECT_EQ(range_length, 3);
  EXPECT_STREQ(range[0], "item1");
  EXPECT_STREQ(range[1], "item2");
  EXPECT_STREQ(range[2], "item3");

  cleanup_lrange_result(range, range_length);
}

TEST_F(LinkedListTest, LrangePartialRange) {
  int length;
  int range_length;

  EXPECT_EQ(rpush(list, "item1", &length), 0);
  EXPECT_EQ(rpush(list, "item2", &length), 0);
  EXPECT_EQ(rpush(list, "item3", &length), 0);

  char **range = lrange(list, 1, 2, &range_length);

  ASSERT_NE(range, nullptr);
  EXPECT_EQ(range_length, 2);
  EXPECT_STREQ(range[0], "item2");
  EXPECT_STREQ(range[1], "item3");

  cleanup_lrange_result(range, range_length);
}

TEST_F(LinkedListTest, LrangeInvalidRange) {
  int length;
  int range_length;

  EXPECT_EQ(rpush(list, "item1", &length), 0);
  EXPECT_EQ(rpush(list, "item2", &length), 0);
  EXPECT_EQ(rpush(list, "item3", &length), 0);

  char **range = lrange(list, 3, 5, &range_length);
  EXPECT_EQ(range, nullptr);
  EXPECT_EQ(range_length, 0);
}

TEST_F(LinkedListTest, LrangeSingleElementRange) {
  int length;
  int range_length;

  EXPECT_EQ(rpush(list, "item1", &length), 0);
  EXPECT_EQ(rpush(list, "item2", &length), 0);
  EXPECT_EQ(rpush(list, "item3", &length), 0);

  char **range = lrange(list, 0, 0, &range_length);

  EXPECT_NE(range, nullptr);
  EXPECT_EQ(range_length, 1);
  EXPECT_STREQ(range[0], "item1");

  cleanup_lrange_result(range, range_length);
}

TEST_F(LinkedListTest, LrangeNegativeIndices) {
  int length;
  int range_length;

  EXPECT_EQ(rpush(list, "item1", &length), 0);
  EXPECT_EQ(rpush(list, "item2", &length), 0);
  EXPECT_EQ(rpush(list, "item3", &length), 0);

  char **range = lrange(list, -3, 2, &range_length);
  ASSERT_NE(range, nullptr);
  EXPECT_EQ(range_length, 3);
  EXPECT_STREQ(range[0], "item1");
  EXPECT_STREQ(range[1], "item2");
  EXPECT_STREQ(range[2], "item3");

  cleanup_lrange_result(range, range_length);

  range = lrange(list, -3, -2, &range_length);
  ASSERT_NE(range, nullptr);
  EXPECT_EQ(range_length, 2);
  EXPECT_STREQ(range[0], "item1");
  EXPECT_STREQ(range[1], "item2");

  cleanup_lrange_result(range, range_length);
}