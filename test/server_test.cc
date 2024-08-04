extern "C" {
#include "server.h"
}
#include <gtest/gtest.h>

// test fixture class for setting and tearing down common test resources
class ServerTest : public ::testing::Test {
 protected:
  khash_t(redis_hash) * h;

  void SetUp() override { h = kh_init(redis_hash); }

  void TearDown() override { cleanup_hash(h); }
};

TEST_F(ServerTest, SetValueString) {
  const char* key = "test_key";
  const char* value = "test_value";
  set_value(h, key, value, TYPE_STRING);

  // verify the value was set correctly
  khiter_t k = kh_get(redis_hash, h, key);
  ASSERT_NE(k, kh_end(h));
  EXPECT_EQ(kh_value(h, k).type, TYPE_STRING);
  EXPECT_STREQ(kh_value(h, k).data.str, value);
}

TEST_F(ServerTest, OverwriteExistingStringValue) {
  const char* key = "test_key";
  const char* initial_value = "initial_value";
  const char* new_value = "new_value";

  // set the initial value
  set_value(h, key, initial_value, TYPE_STRING);

  // verify the initial value was set correctly
  khiter_t k = kh_get(redis_hash, h, key);
  ASSERT_NE(k, kh_end(h));
  EXPECT_EQ(kh_value(h, k).type, TYPE_STRING);
  EXPECT_STREQ(kh_value(h, k).data.str, initial_value);

  // overwrite with the new value
  set_value(h, key, new_value, TYPE_STRING);
  k = kh_get(redis_hash, h, key);  // re-fetch the iterator
  ASSERT_NE(k, kh_end(h));
  EXPECT_EQ(kh_value(h, k).type, TYPE_STRING);
  EXPECT_STREQ(kh_value(h, k).data.str, new_value);
}
