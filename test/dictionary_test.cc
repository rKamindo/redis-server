extern "C" {
#include "server.h"
}
#include <gtest/gtest.h>

// test fixture class for setting and tearing down common test resources
class DictionaryTest : public ::testing::Test {
protected:
  khash_t(redis_hash) * h;

  void SetUp() override { h = kh_init(redis_hash); }

  void TearDown() override { cleanup_hash(h); }
};

TEST_F(DictionaryTest, SetValueString) {
  const char *key = "test_key";
  const char *value = "test_value";
  set_value(h, key, value, TYPE_STRING);

  khiter_t k = kh_get(redis_hash, h, key);
  ASSERT_NE(k, kh_end(h));
  RedisValue *rv = kh_value(h, k);
  EXPECT_EQ(rv->type, TYPE_STRING);
  EXPECT_STREQ(rv->data.str, value);
}

TEST_F(DictionaryTest, OverwriteExistingStringValue) {
  const char *key = "test_key";
  const char *initial_value = "initial_value";
  const char *new_value = "new_value";

  set_value(h, key, initial_value, TYPE_STRING);

  khiter_t k = kh_get(redis_hash, h, key);
  ASSERT_NE(k, kh_end(h));
  RedisValue *rv = kh_value(h, k);
  EXPECT_EQ(rv->type, TYPE_STRING);
  EXPECT_STREQ(rv->data.str, initial_value);

  set_value(h, key, new_value, TYPE_STRING);
  k = kh_get(redis_hash, h, key);
  ASSERT_NE(k, kh_end(h));
  rv = kh_value(h, k);
  EXPECT_EQ(rv->type, TYPE_STRING);
  EXPECT_STREQ(rv->data.str, new_value);
}

TEST_F(DictionaryTest, GetValueString) {
  const char *key = "test_key";
  const char *value = "test_value";
  set_value(h, key, value, TYPE_STRING);

  RedisValue *retrieved_value = get_value(h, key);
  ASSERT_NE(retrieved_value, nullptr);
  EXPECT_EQ(retrieved_value->type, TYPE_STRING);
  EXPECT_STREQ(retrieved_value->data.str, value);
}

TEST_F(DictionaryTest, GetValueKeyNotFound) {
  RedisValue *retrieved_value = get_value(h, "test_key");
  ASSERT_EQ(retrieved_value, nullptr);
}