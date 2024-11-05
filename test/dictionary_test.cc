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
  set_value(h, key, value, TYPE_STRING, 0);

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

  set_value(h, key, initial_value, TYPE_STRING, 0);

  khiter_t k = kh_get(redis_hash, h, key);
  ASSERT_NE(k, kh_end(h));
  RedisValue *rv = kh_value(h, k);
  EXPECT_EQ(rv->type, TYPE_STRING);
  EXPECT_STREQ(rv->data.str, initial_value);

  set_value(h, key, new_value, TYPE_STRING, 0);
  k = kh_get(redis_hash, h, key);
  ASSERT_NE(k, kh_end(h));
  rv = kh_value(h, k);
  EXPECT_EQ(rv->type, TYPE_STRING);
  EXPECT_STREQ(rv->data.str, new_value);
}

TEST_F(DictionaryTest, GetValueString) {
  const char *key = "test_key";
  const char *value = "test_value";
  set_value(h, key, value, TYPE_STRING, 0);

  RedisValue *retrieved_value = get_value(h, key);
  ASSERT_NE(retrieved_value, nullptr);
  EXPECT_EQ(retrieved_value->type, TYPE_STRING);
  EXPECT_STREQ(retrieved_value->data.str, value);
}

TEST_F(DictionaryTest, GetValueKeyNotFound) {
  RedisValue *retrieved_value = get_value(h, "test_key");
  ASSERT_EQ(retrieved_value, nullptr);
}

TEST_F(DictionaryTest, SetValueWithExpiration) {
  const char *key = "expiring_key";
  const char *value = "expiring_value";
  long long expiration = current_time_millis() + 1000; // 1 second from now
  set_value(h, key, value, TYPE_STRING, expiration);

  RedisValue *retrieved_value = get_value(h, key);
  ASSERT_NE(retrieved_value, nullptr);
  EXPECT_EQ(retrieved_value->type, TYPE_STRING);
  EXPECT_STREQ(retrieved_value->data.str, value);
  EXPECT_EQ(retrieved_value->expiration, expiration);
}

TEST_F(DictionaryTest, GetExpiredValue) {
  const char *key = "expired_key";
  const char *value = "expired_value";
  long long expiration = current_time_millis() - 1000; // 1 second ago
  set_value(h, key, value, TYPE_STRING, expiration);

  RedisValue *retrieved_value = get_value(h, key);
  ASSERT_EQ(retrieved_value, nullptr);

  // verify that the key has been removed from the hash table
  khiter_t k = kh_get(redis_hash, h, key);
  EXPECT_EQ(k, kh_end(h));
}

TEST_F(DictionaryTest, UpdateExpirationTime) {
  const char *key = "update_expiry_key";
  const char *value = "update_expiry_value";
  long long initial_expiration = current_time_millis() + 1000; // 1 second from now
  set_value(h, key, value, TYPE_STRING, initial_expiration);

  long long new_expiration = current_time_millis() + 5000; // 5 seconds from now
  set_value(h, key, value, TYPE_STRING, new_expiration);

  RedisValue *retrieved_value = get_value(h, key);
  ASSERT_NE(retrieved_value, nullptr);
  EXPECT_EQ(retrieved_value->expiration, new_expiration);
}

TEST_F(DictionaryTest, RemoveExpiration) {
  const char *key = "remove_expiry_key";
  const char *value = "remove_expiry_value";
  long long initial_expiration = current_time_millis() + 1000; // 1 second from now
  set_value(h, key, value, TYPE_STRING, initial_expiration);

  set_value(h, key, value, TYPE_STRING, 0); // set without expiration

  RedisValue *retrieved_value = get_value(h, key);
  ASSERT_NE(retrieved_value, nullptr);
  EXPECT_EQ(retrieved_value->expiration, 0);
}

TEST_F(DictionaryTest, MultipleKeysWithExpiration) {
  const char *key1 = "key1";
  const char *key2 = "key2";
  const char *key3 = "key3";
  const char *value = "value";

  long long now = current_time_millis();
  set_value(h, key1, value, TYPE_STRING, now - 1000); // already expired
  set_value(h, key2, value, TYPE_STRING, now + 1000); // expires in 1 second
  set_value(h, key3, value, TYPE_STRING, 0);          // no expiration

  EXPECT_EQ(get_value(h, key1), nullptr);
  EXPECT_NE(get_value(h, key2), nullptr);
  EXPECT_NE(get_value(h, key3), nullptr);

  // wait for key2 to expire
  usleep(1100000); // sleep for 1.1 seconds

  EXPECT_EQ(get_value(h, key1), nullptr);
  EXPECT_EQ(get_value(h, key2), nullptr);
  EXPECT_NE(get_value(h, key3), nullptr);
}