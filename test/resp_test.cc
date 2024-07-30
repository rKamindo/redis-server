extern "C" {
#include "resp.h"
}
#include <gtest/gtest.h>

TEST(SerializeTest, HandleSimpleString) {
  const char* input = "Ok";
  char* result = serialize_simple_string(input);
  EXPECT_STREQ(result, "+Ok\r\n");
  free(result);
}

TEST(SerializeTest, HandleError) {
  const char* input = "Error message";
  char* result = serialize_error(input);
  EXPECT_STREQ(result, "-Error message\r\n");
  free(result);
}

TEST(SerializeTest, HandleInteger) {
  const int input = 55;
  char* result = serialize_integer(input);
  EXPECT_STREQ(result, ":55\r\n");
  free(result);
}

TEST(SerializeTest, HandleBulkString) {
  const char* input = "Hello";
  char* result = serialize_bulk_string(input);
  EXPECT_STREQ(result, "$5\r\nHello\r\n");
  free(result);
}

TEST(SerializeTest, HandleNullBulkString) {
  char* result = serialize_bulk_string(NULL);
  EXPECT_STREQ(result, "$-1\r\n");
  free(result);
}

TEST(SerializeTest, HandleArrayOfBulkStrings) {
  const char* input[] = {"hello", "world"};
  char* result = serialize_array(input, 2);
  EXPECT_STREQ(result, "*2\r\n$5\r\nhello\r\n$5\r\nworld\r\n");
  free(result);
}

TEST(SerializeTest, HandleEmptyArray) {
  char* result = serialize_array(NULL, 0);
  EXPECT_STREQ(result, "*-1\r\n");
  free(result);
}

TEST(SerializeTest, HandleArrayWithNullElements) {
  const char* input[] = {"hello", NULL, "world"};
  char* result = serialize_array(input, 3);
  EXPECT_STREQ(result, "*3\r\n$5\r\nhello\r\n$-1\r\n$5\r\nworld\r\n");
  free(result);
}

TEST(DeserializeCommandTest, ValidCommand) {
  const char* input = "*2\r\n$4\r\necho\r\n$11\r\nhello world\r\n";
  int count;
  char** result = deserialize_command(input, &count);

  ASSERT_NE(result, nullptr);
  EXPECT_EQ(count, 2);
  EXPECT_STREQ(result[0], "echo");
  EXPECT_STREQ(result[1], "hello world");

  free_command(result, count);
}

TEST(DeserializeCommandTest, NullBulkString) {
  const char* input = "*3\r\n$3\r\nget\r\n$-1\r\n$4\r\nName\r\n";
  int count;
  char** result = deserialize_command(input, &count);

  ASSERT_NE(result, nullptr);
  EXPECT_STREQ(result[0], "get");
  EXPECT_EQ(result[1], nullptr);
  EXPECT_STREQ(result[2], "Name");

  free_command(result, count);
}