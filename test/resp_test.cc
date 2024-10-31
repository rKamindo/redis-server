extern "C" {
#include "mock_handler.h"
#include "resp.h"
}
#include <gtest/gtest.h>

class ParseTest : public ::testing::Test {
protected:
  Handler *mock_handler;
  CommandHandler *mock_ch;
  Parser parser;

  void SetUp() override {
    mock_handler = create_mock_handler();
    mock_ch = create_mock_command_handler();
    parser_init(&parser, mock_handler, mock_ch);
  }

  void TearDown() override {
    destroy_mock_handler(mock_handler);
    destroy_mock_command_handler(mock_ch);
  }
};

TEST_F(ParseTest, SimpleString) {
  const char *input = "+hello world\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
}

TEST_F(ParseTest, SimpleError) {
  const char *input = "-bad error";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
}

TEST_F(ParseTest, Integer_SmallValue) {
  const char *input = ":0\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
}

TEST_F(ParseTest, Integer_LargeValue) {
  const char *input = ":12345678\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
}

TEST_F(ParseTest, BulkString_ValidInput) {
  const char *input = "$10\r\nabcdefghir\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
}

TEST_F(ParseTest, BulkString_NullBulkString) {
  const char *input = "$-1\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
}

TEST_F(ParseTest, BulkString_EmptyInput) {
  const char *input = "$0\r\n\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
}

TEST_F(ParseTest, Array_ValidInput) {
  const char *input = "*4\r\n$4\r\necho\r\n$11\r\nhello world\r\n$4\r\necho\r\n$2\r\nhi\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
}

TEST_F(ParseTest, Array_NullValue) {
  const char *input = "*-1\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
}

TEST_F(ParseTest, InlineCommand) {
  const char *input = "SET KEY VALUE";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
}