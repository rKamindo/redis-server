extern "C" {
#include "database.h"
#include "util.h"
}
#include <gtest/gtest.h>
#include <unistd.h>

class DatabaseTest : public ::testing::Test {
protected:
  void SetUp() override { redis_db_create(); }
  void TearDown() override { redis_db_destroy(); }
};

TEST_F(DatabaseTest, SetValueString) {
  const char *key = "test_key";
  const char *value = "test_value";
  redis_db_set(key, value, TYPE_STRING, 0);

  RedisValue *rv = redis_db_get(key);
  ASSERT_NE(rv, nullptr);
  EXPECT_EQ(rv->type, TYPE_STRING);
  EXPECT_STREQ(rv->data.str, value);
}

TEST_F(DatabaseTest, OverwriteExistingStringValue) {
  const char *key = "test_key";
  const char *initial_value = "initial_value";
  const char *new_value = "new_value";

  redis_db_set(key, initial_value, TYPE_STRING, 0);

  RedisValue *rv = redis_db_get(key);
  ASSERT_NE(rv, nullptr);
  EXPECT_EQ(rv->type, TYPE_STRING);
  EXPECT_STREQ(rv->data.str, initial_value);

  redis_db_set(key, new_value, TYPE_STRING, 0);
  rv = redis_db_get(key);
  ASSERT_NE(rv, nullptr);
  EXPECT_EQ(rv->type, TYPE_STRING);
  EXPECT_STREQ(rv->data.str, new_value);
}

TEST_F(DatabaseTest, GetValueString) {
  const char *key = "test_key";
  const char *value = "test_value";
  redis_db_set(key, value, TYPE_STRING, 0);

  RedisValue *retrieved_value = redis_db_get(key);
  ASSERT_NE(retrieved_value, nullptr);
  EXPECT_EQ(retrieved_value->type, TYPE_STRING);
  EXPECT_STREQ(retrieved_value->data.str, value);
}

TEST_F(DatabaseTest, GetValueKeyNotFound) {
  RedisValue *retrieved_value = redis_db_get("non_existent_key");
  ASSERT_EQ(retrieved_value, nullptr);
}

TEST_F(DatabaseTest, SetValueWithExpiration) {
  const char *key = "expiring_key";
  const char *value = "expiring_value";
  long long expiration = current_time_millis() + 1000; // 1 second from now
  redis_db_set(key, value, TYPE_STRING, expiration);

  RedisValue *retrieved_value = redis_db_get(key);
  ASSERT_NE(retrieved_value, nullptr);
  EXPECT_EQ(retrieved_value->type, TYPE_STRING);
  EXPECT_STREQ(retrieved_value->data.str, value);
  EXPECT_EQ(retrieved_value->expiration, expiration);
}

TEST_F(DatabaseTest, GetExpiredValue) {
  const char *key = "expired_key";
  const char *value = "expired_value";
  long long expiration = current_time_millis() - 1000; // 1 second ago
  redis_db_set(key, value, TYPE_STRING, expiration);

  RedisValue *retrieved_value = redis_db_get(key);
  ASSERT_EQ(retrieved_value, nullptr);
}

TEST_F(DatabaseTest, UpdateExpirationTime) {
  const char *key = "update_expiry_key";
  const char *value = "update_expiry_value";
  long long initial_expiration = current_time_millis() + 1000; // 1 second from now
  redis_db_set(key, value, TYPE_STRING, initial_expiration);

  long long new_expiration = current_time_millis() + 5000; // 5 seconds from now
  redis_db_set(key, value, TYPE_STRING, new_expiration);

  RedisValue *retrieved_value = redis_db_get(key);
  ASSERT_NE(retrieved_value, nullptr);
  EXPECT_EQ(retrieved_value->expiration, new_expiration);
}

TEST_F(DatabaseTest, RemoveExpirationAndPersist) {
  const char *key = "remove_expiry_key";
  const char *value = "remove_expiry_value";
  long long initial_expiration = current_time_millis() + 1000; // 1 second from now
  redis_db_set(key, value, TYPE_STRING, initial_expiration);

  redis_db_set(key, value, TYPE_STRING, 0); // set without expiration
  usleep(1100000); // sleep for 1.1 seconds, to verify if key persists after removing expiration

  RedisValue *retrieved_value = redis_db_get(key);
  ASSERT_NE(retrieved_value, nullptr);
  EXPECT_EQ(retrieved_value->expiration, 0);
}

TEST_F(DatabaseTest, MultipleKeysWithExpiration) {
  const char *key1 = "key1";
  const char *key2 = "key2";
  const char *key3 = "key3";
  const char *value = "value";

  long long now = current_time_millis();
  redis_db_set(key1, value, TYPE_STRING, now - 1000); // already expired
  redis_db_set(key2, value, TYPE_STRING, now + 1000); // expires in 1 second
  redis_db_set(key3, value, TYPE_STRING, 0);          // no expiration

  EXPECT_EQ(redis_db_get(key1), nullptr);
  EXPECT_NE(redis_db_get(key2), nullptr);
  EXPECT_NE(redis_db_get(key3), nullptr);

  // wait for key2 to expire
  usleep(1100000); // sleep for 1.1 seconds

  EXPECT_EQ(redis_db_get(key1), nullptr);
  EXPECT_EQ(redis_db_get(key2), nullptr);
  EXPECT_NE(redis_db_get(key3), nullptr);
}

TEST_F(DatabaseTest, SetAndCheckExists) {
  const char *key = "test_key";
  const char *value = "test_value";
  redis_db_set(key, value, TYPE_STRING, 0);

  bool exists = redis_db_exist(key);

  EXPECT_EQ(exists, true);
}

TEST_F(DatabaseTest, DoNotSetKeyShouldNotExist) {
  const char *key = "test_key";

  bool exists = redis_db_exist(key);

  EXPECT_EQ(exists, false);
}

TEST_F(DatabaseTest, DeleteShouldNotExist) {
  const char *key = "test_key";
  const char *value = "test_value";
  redis_db_set(key, value, TYPE_STRING, 0);
  redis_db_delete(key);

  bool exists = redis_db_exist(key);

  EXPECT_EQ(exists, false);
}