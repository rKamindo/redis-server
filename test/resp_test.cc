extern "C" {
#include "client.h"
#include "handler.h"
#include "mock_handler.h"
#include "resp.h"
}
#include <gtest/gtest.h>

class ParseTest : public ::testing::Test {
protected:
  CommandHandler *mock_ch;
  Parser parser;
  void SetUp() override {
    g_handler = create_mock_handler();
    mock_ch = create_mock_command_handler();
    parser_init(&parser, mock_ch);
  }

  void TearDown() override {
    destroy_mock_command_handler(mock_ch);
    destroy_mock_handler(g_handler);
  }
};

TEST_F(ParseTest, SimpleString) {
  const char *input = "+hello world\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
  ASSERT_EQ(parser.stack_top, 0);
}
TEST_F(ParseTest, SimpleError) {
  const char *input = "-good error\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
  ASSERT_EQ(parser.stack_top, 0);
}

TEST_F(ParseTest, Integer_SmallValue) {
  const char *input = ":0\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
  ASSERT_EQ(parser.stack_top, 0);
}

TEST_F(ParseTest, Integer_LargeValue) {
  const char *input = ":12345678\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
  ASSERT_EQ(parser.stack_top, 0);
}

TEST_F(ParseTest, BulkString_ValidInput) {
  const char *input = "$10\r\nabcdefghij\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
  ASSERT_EQ(parser.stack_top, 0);
}

TEST_F(ParseTest, BulkString_NullBulkString) {
  const char *input = "$-1\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
  ASSERT_EQ(parser.stack_top, 0);
}

TEST_F(ParseTest, BulkString_EmptyInput) {
  const char *input = "$0\r\n\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
  ASSERT_EQ(parser.stack_top, 0);
}

TEST_F(ParseTest, Array_ValidInput) {
  const char *input = "*4\r\n$4\r\necho\r\n$11\r\nhello world\r\n$4\r\necho\r\n$2\r\nhi\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
  ASSERT_EQ(parser.stack_top, 0);
}

TEST_F(ParseTest, Array_NullValue) {
  const char *input = "*-1\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
  ASSERT_EQ(parser.stack_top, 0);
}

TEST_F(ParseTest, InlineCommand_Set) {
  const char *input = "SET KEY VALUE\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
  ASSERT_EQ(parser.stack_top, 0);
}

TEST_F(ParseTest, InlineCommand_Echo_MultiWordArgument_InQuotes) {
  const char *input = "ECHO 'HELLO WORLD'\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
  ASSERT_EQ(parser.stack_top, 0);
}

TEST_F(ParseTest, InlineCommand_Echo_SingleWordArgument) {
  const char *input = "ECHO HELLO\r\n";

  const char *end = parser_parse(&parser, input, input + strlen(input));

  ASSERT_EQ(end, input + strlen(input));
  ASSERT_EQ(parser.stack_top, 0);
}

TEST_F(ParseTest, NullCommandHandlerDoesNotCrash) {
  // parser should handle NULL command handler without crashing
  parser.command_handler = NULL;
  const char *input = "+hello\r\n";
  const char *end = parser_parse(&parser, input, input + strlen(input));
  ASSERT_EQ(end, input + strlen(input));
  ASSERT_EQ(parser.stack_top, 0);
}

TEST_F(ParseTest, MasterWithFullResyncStateDoesNotConsumeBytes) {
  // when master has received fullresync response, parser should not consume bytes
  CommandHandler *ch = create_mock_command_handler();
  Client *client = (Client *)malloc(sizeof(Client));
  memset(client, 0, sizeof(Client));
  client->type = CLIENT_TYPE_MASTER;
  client->repl_client_state = REPL_STATE_RECEIVED_FULLRESYNC_RESPONSE;
  ch->client = client;

  parser.command_handler = ch;
  const char *input = "+hello\r\n";
  const char *end = parser_parse(&parser, input, input + strlen(input));
  // parser should return begin (no bytes consumed)
  ASSERT_EQ(end, input);
  free(client);
  free(ch);
}