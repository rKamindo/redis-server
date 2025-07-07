extern "C" {
#include "../src/database.h"
#include "../src/util.h"
}
#include <gtest/gtest.h>
#include <unistd.h>

redis_db_t *db;

class DatabaseTest : public ::testing::Test {
protected:
  void SetUp() override { db = redis_db_create(); }
  void TearDown() override { redis_db_destroy(db); }
};

TEST_F(DatabaseTest, SetValueString) {
  const char *key = "test_key";
  const char *value = "test_value";
  redis_db_set(db, key, value, TYPE_STRING, 0);

  RedisValue *rv = redis_db_get(db, key);
  ASSERT_NE(rv, nullptr);
  EXPECT_EQ(rv->type, TYPE_STRING);
  EXPECT_STREQ(rv->data.str, value);
  EXPECT_EQ(db->key_count, 1);
  EXPECT_EQ(db->expiry_count, 0);
}

TEST_F(DatabaseTest, OverwriteExistingStringValue) {
  const char *key = "test_key";
  const char *initial_value = "initial_value";
  const char *new_value = "new_value";

  redis_db_set(db, key, initial_value, TYPE_STRING, 0);

  RedisValue *rv = redis_db_get(db, key);
  ASSERT_NE(rv, nullptr);
  EXPECT_EQ(rv->type, TYPE_STRING);
  EXPECT_STREQ(rv->data.str, initial_value);
  EXPECT_EQ(db->key_count, 1);

  redis_db_set(db, key, new_value, TYPE_STRING, 0);
  rv = redis_db_get(db, key);
  ASSERT_NE(rv, nullptr);
  EXPECT_EQ(rv->type, TYPE_STRING);
  EXPECT_STREQ(rv->data.str, new_value);
  EXPECT_EQ(db->key_count, 1);
  EXPECT_EQ(db->expiry_count, 0);
}

TEST_F(DatabaseTest, GetValueString) {
  const char *key = "test_key";
  const char *value = "test_value";
  redis_db_set(db, key, value, TYPE_STRING, 0);

  RedisValue *retrieved_value = redis_db_get(db, key);
  ASSERT_NE(retrieved_value, nullptr);
  EXPECT_EQ(retrieved_value->type, TYPE_STRING);
  EXPECT_STREQ(retrieved_value->data.str, value);
}

TEST_F(DatabaseTest, GetValueKeyNotFound) {
  RedisValue *retrieved_value = redis_db_get(db, "non_existent_key");
  ASSERT_EQ(retrieved_value, nullptr);
}

TEST_F(DatabaseTest, SetValueWithExpiration) {
  const char *key = "expiring_key";
  const char *value = "expiring_value";
  long long expiration = current_time_millis() + 1000; // 1 second from now
  redis_db_set(db, key, value, TYPE_STRING, expiration);

  RedisValue *retrieved_value = redis_db_get(db, key);
  ASSERT_NE(retrieved_value, nullptr);
  EXPECT_EQ(retrieved_value->type, TYPE_STRING);
  EXPECT_STREQ(retrieved_value->data.str, value);
  EXPECT_EQ(retrieved_value->expiration, expiration);
  EXPECT_EQ(db->key_count, 1);
  EXPECT_EQ(db->expiry_count, 1);
}

TEST_F(DatabaseTest, GetExpiredValue) {
  const char *key = "expired_key";
  const char *value = "expired_value";
  long long expiration = current_time_millis() - 1000; // 1 second ago
  redis_db_set(db, key, value, TYPE_STRING, expiration);

  RedisValue *retrieved_value = redis_db_get(db, key);
  ASSERT_EQ(retrieved_value, nullptr);
  EXPECT_EQ(db->key_count, 0);
  EXPECT_EQ(db->expiry_count, 0);
}

TEST_F(DatabaseTest, UpdateExpirationTime) {
  const char *key = "update_expiry_key";
  const char *value = "update_expiry_value";
  long long initial_expiration = current_time_millis() + 1000; // 1 second from now
  redis_db_set(db, key, value, TYPE_STRING, initial_expiration);

  long long new_expiration = current_time_millis() + 5000; // 5 seconds from now
  redis_db_set(db, key, value, TYPE_STRING, new_expiration);

  RedisValue *retrieved_value = redis_db_get(db, key);
  ASSERT_NE(retrieved_value, nullptr);
  EXPECT_EQ(retrieved_value->expiration, new_expiration);
}

TEST_F(DatabaseTest, RemoveExpirationAndPersist) {
  const char *key = "remove_expiry_key";
  const char *value = "remove_expiry_value";
  long long initial_expiration = current_time_millis() + 1000; // 1 second from now
  redis_db_set(db, key, value, TYPE_STRING, initial_expiration);

  redis_db_set(db, key, value, TYPE_STRING, 0); // set without expiration
  usleep(1100000); // sleep for 1.1 seconds, to verify if key persists after removing expiration

  RedisValue *retrieved_value = redis_db_get(db, key);
  ASSERT_NE(retrieved_value, nullptr);
  EXPECT_EQ(retrieved_value->expiration, 0);
}

TEST_F(DatabaseTest, MultipleKeysWithExpiration) {
  const char *key1 = "key1";
  const char *key2 = "key2";
  const char *key3 = "key3";
  const char *value = "value";

  long long now = current_time_millis();
  redis_db_set(db, key1, value, TYPE_STRING, now - 1000); // already expired
  redis_db_set(db, key2, value, TYPE_STRING, now + 1000); // expires in 1 second
  redis_db_set(db, key3, value, TYPE_STRING, 0);          // no expiration

  EXPECT_EQ(redis_db_get(db, key1), nullptr);
  EXPECT_NE(redis_db_get(db, key2), nullptr);
  EXPECT_NE(redis_db_get(db, key3), nullptr);

  // wait for key2 to expire
  usleep(1100000); // sleep for 1.1 seconds

  EXPECT_EQ(redis_db_get(db, key1), nullptr);
  EXPECT_EQ(redis_db_get(db, key2), nullptr);
  EXPECT_NE(redis_db_get(db, key3), nullptr);
}

TEST_F(DatabaseTest, SetAndCheckExists) {
  const char *key = "test_key";
  const char *value = "test_value";
  redis_db_set(db, key, value, TYPE_STRING, 0);

  bool exists = redis_db_exist(db, key);

  EXPECT_EQ(exists, true);
}

TEST_F(DatabaseTest, DoNotSetKeyShouldNotExist) {
  const char *key = "test_key";

  bool exists = redis_db_exist(db, key);

  EXPECT_EQ(exists, false);
}

TEST_F(DatabaseTest, DeleteShouldNotExist) {
  const char *key = "test_key";
  const char *value = "test_value";
  redis_db_set(db, key, value, TYPE_STRING, 0);
  redis_db_delete(db, key);

  bool exists = redis_db_exist(db, key);

  EXPECT_EQ(exists, false);
}

TEST_F(DatabaseTest, LPushOperation) {
  const char *key = "list_key";
  const char *value1 = "value1";
  const char *value2 = "value2";
  int length;

  EXPECT_EQ(redis_db_lpush(db, key, value1, &length), 0);
  EXPECT_EQ(length, 1);

  EXPECT_EQ(redis_db_lpush(db, key, value2, &length), 0);
  EXPECT_EQ(length, 2);

  RedisValue *rv = redis_db_get(db, key);
  ASSERT_NE(rv, nullptr);
  EXPECT_EQ(rv->type, TYPE_LIST);
  EXPECT_EQ(get_list_length(rv->data.list), 2);
}

TEST_F(DatabaseTest, RPushOperation) {
  const char *key = "list_key";
  const char *value1 = "value1";
  const char *value2 = "value2";
  int length;

  EXPECT_EQ(redis_db_rpush(db, key, value1, &length), 0);
  EXPECT_EQ(length, 1);

  EXPECT_EQ(redis_db_rpush(db, key, value2, &length), 0);
  EXPECT_EQ(length, 2);

  RedisValue *rv = redis_db_get(db, key);
  ASSERT_NE(rv, nullptr);
  EXPECT_EQ(rv->type, TYPE_LIST);
  EXPECT_EQ(get_list_length(rv->data.list), 2);
}

TEST_F(DatabaseTest, LPushRPushCombined) {
  const char *key = "list_key";
  const char *value1 = "value1";
  const char *value2 = "value2";
  int length;

  EXPECT_EQ(redis_db_lpush(db, key, value1, &length), 0);
  EXPECT_EQ(redis_db_rpush(db, key, value2, &length), 0);
  EXPECT_EQ(length, 2);

  RedisValue *rv = redis_db_get(db, key);
  ASSERT_NE(rv, nullptr);
  EXPECT_EQ(rv->type, TYPE_LIST);
  EXPECT_EQ(get_list_length(rv->data.list), 2);
}

TEST_F(DatabaseTest, PushToNonListKey) {
  const char *key = "string_key";
  const char *string_value = "string_value";
  const char *list_value = "list_value";
  int length;

  redis_db_set(db, key, string_value, TYPE_STRING, 0);

  EXPECT_EQ(redis_db_lpush(db, key, list_value, &length), ERR_TYPE_MISMATCH);
  EXPECT_EQ(redis_db_rpush(db, key, list_value, &length), ERR_TYPE_MISMATCH);

  RedisValue *rv = redis_db_get(db, key);
  ASSERT_NE(rv, nullptr);
  EXPECT_EQ(rv->type, TYPE_STRING);
  EXPECT_STREQ(rv->data.str, string_value);
}

TEST_F(DatabaseTest, DeleteList) {
  const char *key = "list_key";
  const char *value = "value";
  int length;

  EXPECT_EQ(redis_db_lpush(db, key, value, &length), 0);
  EXPECT_EQ(length, 1);

  redis_db_delete(db, key);

  EXPECT_FALSE(redis_db_exist(db, key));
}

TEST_F(DatabaseTest, LrangeValidRange) {
  const char *key = "list_key";
  const char *value1 = "value1";
  const char *value2 = "value2";
  const char *value3 = "value3";
  int length;

  EXPECT_EQ(redis_db_rpush(db, key, value1, &length), 0);
  EXPECT_EQ(redis_db_rpush(db, key, value2, &length), 0);
  EXPECT_EQ(redis_db_rpush(db, key, value3, &length), 0);
  EXPECT_EQ(length, 3);

  char **range;
  int range_length;
  int result = redis_db_lrange(db, key, 0, 2, &range, &range_length);
  EXPECT_EQ(result, 0); // check for success
  EXPECT_NE(range, nullptr);
  EXPECT_EQ(range[0], std::string(value1));
  EXPECT_EQ(range[1], std::string(value2));
  EXPECT_EQ(range[2], std::string(value3));

  cleanup_lrange_result(range, range_length);

  // test partial range
  result = redis_db_lrange(db, key, 0, 1, &range, &range_length);
  EXPECT_EQ(result, 0); // check for success
  EXPECT_NE(range, nullptr);
  EXPECT_EQ(range[0], std::string(value1));
  EXPECT_EQ(range[1], std::string(value2));

  cleanup_lrange_result(range, range_length);

  // test range end out of bounds
  result = redis_db_lrange(db, key, 0, 5, &range, &range_length);
  EXPECT_EQ(result, 0); // check for success
  EXPECT_NE(range, nullptr);
  EXPECT_EQ(range[0], std::string(value1));
  EXPECT_EQ(range[1], std::string(value2));
  EXPECT_EQ(range[2], std::string(value3));

  cleanup_lrange_result(range, range_length);

  // test range negative indices
  result = redis_db_lrange(db, key, -3, -1, &range, &range_length);
  EXPECT_EQ(result, 0); // check for success
  EXPECT_NE(range, nullptr);
  EXPECT_EQ(range[0], std::string(value1));
  EXPECT_EQ(range[1], std::string(value2));
  EXPECT_EQ(range[2], std::string(value3));

  cleanup_lrange_result(range, range_length); // Clean up again

  redis_db_delete(db, key); // delete the list
  result = redis_db_lrange(db, key, 0, 2, &range, &range_length);
  EXPECT_EQ(result, ERR_KEY_NOT_FOUND); // Expect an error for non-existent key
}