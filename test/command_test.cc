#include <gtest/gtest.h>
extern "C" {
#include "client.h"
#include "command_handler.h"
#include "commands.h"
#include "database.h"
}

class CommandTest : public ::testing::Test {
protected:
  void SetUp() override {
    redis_db_create();
    client = create_client(-1, nullptr); // -1 as we don't need a real socket for tests
    ch = create_command_handler(client, 1024, 10);
  }

  void TearDown() override {
    destroy_command_handler(ch);
    destroy_client(client);
    redis_db_destroy();
  }

  void ExecuteCommand(const std::vector<std::string> &args) {
    ch->arg_count = args.size();
    for (size_t i = 0; i < args.size(); ++i) {
      ch->args[i] = strdup(args[i].c_str());
    }
    handle_command(ch);
    for (size_t i = 0; i < args.size(); ++i) {
      free(ch->args[i]);
    }
  }

  // used to consume a reply out of the output buffer
  std::string GetReply() {
    char *buf;
    size_t len;
    rb_readable(client->output_buffer, &buf, &len);
    std::string reply(buf, len);
    rb_read(client->output_buffer, len);
    return reply;
  }

  Client *client;
  CommandHandler *ch;
};

TEST_F(CommandTest, PingCommand) {
  ExecuteCommand({"PING"});
  EXPECT_EQ(GetReply(), "+PONG\r\n");

  ExecuteCommand({"PING", "Hello"});
  EXPECT_EQ(GetReply(), "+Hello\r\n");
}

TEST_F(CommandTest, EchoCommand) {
  ExecuteCommand({"ECHO", "Hello, World!"});
  EXPECT_EQ(GetReply(), "+Hello, World!\r\n");
}

TEST_F(CommandTest, SetCommand) {
  ExecuteCommand({"SET", "mykey", "myvalue"});
  EXPECT_EQ(GetReply(), "+OK\r\n");

  ExecuteCommand({"GET", "mykey"});
  EXPECT_EQ(GetReply(), "$7\r\nmyvalue\r\n");
}

TEST_F(CommandTest, SetWithNX_ShouldNotOverwriteExistingKey) {
  ExecuteCommand({"SET", "mykey", "initial_value"});
  EXPECT_EQ(GetReply(), "+OK\r\n");

  ExecuteCommand({"SET", "mykey", "new_value", "NX"});
  EXPECT_EQ(GetReply(), "$-1\r\n");

  ExecuteCommand({"GET", "mykey"});
  EXPECT_EQ(GetReply(), "$13\r\ninitial_value\r\n");
}

TEST_F(CommandTest, SetWithXX_ShouldOverwriteExistingKey) {
  ExecuteCommand({"SET", "mykey", "initial_value"});
  EXPECT_EQ(GetReply(), "+OK\r\n");

  ExecuteCommand({"SET", "mykey", "new_value", "XX"});
  EXPECT_EQ(GetReply(), "+OK\r\n"); // should overwrite

  ExecuteCommand({"GET", "mykey"});
  EXPECT_EQ(GetReply(), "$9\r\nnew_value\r\n"); // should now be new_value
}

TEST_F(CommandTest, SetWithXX_ShouldNotSet) {
  ExecuteCommand({"SET", "mykey", "new_value", "XX"});
  EXPECT_EQ(GetReply(), "$-1\r\n"); // should not set, key does not exist

  ExecuteCommand({"GET", "mykey"});
  EXPECT_EQ(GetReply(), "$-1\r\n");
}

TEST_F(CommandTest, SetWithGET_ShouldReturnOldValue) {
  ExecuteCommand({"SET", "mykey", "initial_value"});
  EXPECT_EQ(GetReply(), "+OK\r\n");

  ExecuteCommand({"SET", "mykey", "new_value", "GET"});
  EXPECT_EQ(GetReply(), "$13\r\ninitial_value\r\n"); // Should return old value
}

TEST_F(CommandTest, SetWithGET_ShouldReturnNull) {
  ExecuteCommand({"SET", "mykey", "initial_value", "GET"});
  EXPECT_EQ(GetReply(), "$-1\r\n");
}

TEST_F(CommandTest, SetWithExpiration_ShouldExpireKey) {
  ExecuteCommand({"SET", "expiring_key", "value", "EX", "1"}); // 1 second expiration
  EXPECT_EQ(GetReply(), "+OK\r\n");

  usleep(1100000); // sleep for 1.1 seconds to allow expiration

  ExecuteCommand({"GET", "expiring_key"});
  EXPECT_EQ(GetReply(), "$-1\r\n"); // should return null since it expired
}

TEST_F(CommandTest, SetWithKEEPTTL_ShouldPreserveTTL) {
  ExecuteCommand({"SET", "ttl_key", "value", "EX", "2"}); // 2 seconds expiration
  EXPECT_EQ(GetReply(), "+OK\r\n");

  usleep(1000000); // sleep for 1 second

  ExecuteCommand({"SET", "ttl_key", "new_value", "KEEPTTL"}); // update with KEEPTTL
  EXPECT_EQ(GetReply(), "+OK\r\n");

  usleep(1200000); // sleep for another 1.2 seconds to check expiration

  ExecuteCommand({"GET", "ttl_key"});
  EXPECT_EQ(GetReply(), "$-1\r\n"); // should return null since it expired
}

TEST_F(CommandTest, GetCommand) {
  ExecuteCommand({"SET", "mykey", "myvalue"});
  GetReply();

  ExecuteCommand({"GET", "mykey"});
  EXPECT_EQ(GetReply(), "$7\r\nmyvalue\r\n");

  ExecuteCommand({"GET", "nonexistent"});
  EXPECT_EQ(GetReply(), "$-1\r\n");
}

TEST_F(CommandTest, ExistCommand) {
  ExecuteCommand({"SET", "key1", "value1"});
  GetReply();
  ExecuteCommand({"SET", "key2", "value2"});
  GetReply();

  ExecuteCommand({"EXIST", "key1", "key2", "nonexistent"});
  EXPECT_EQ(GetReply(), ":2\r\n");
}

TEST_F(CommandTest, DeleteCommand) {
  ExecuteCommand({"SET", "key1", "value1"});
  GetReply();
  ExecuteCommand({"SET", "key2", "value2"});
  GetReply();

  ExecuteCommand({"DEL", "key1", "key2", "nonexistent"});
  EXPECT_EQ(GetReply(), "+OK\r\n");

  ExecuteCommand({"GET", "key1"});
  EXPECT_EQ(GetReply(), "$-1\r\n");
  ExecuteCommand({"GET", "key2"});
  EXPECT_EQ(GetReply(), "$-1\r\n");
}

TEST_F(CommandTest, IncrCommand) {
  ExecuteCommand({"SET", "mykey", "10"});
  EXPECT_EQ(GetReply(), "+OK\r\n");

  ExecuteCommand({"INCR", "mykey"});
  EXPECT_EQ(GetReply(), ":11\r\n");

  ExecuteCommand({"INCR", "mykey"});
  EXPECT_EQ(GetReply(), ":12\r\n");

  ExecuteCommand({"GET", "mykey"});
  EXPECT_EQ(GetReply(), "$2\r\n12\r\n");
}

TEST_F(CommandTest, IncrNonExistentKey) {
  ExecuteCommand({"INCR", "key"});
  EXPECT_EQ(GetReply(), ":1\r\n"); // should initialize and increment to 1

  ExecuteCommand({"GET", "key"});
  EXPECT_EQ(GetReply(), "$1\r\n1\r\n"); // should return 1
}

TEST_F(CommandTest, DecrCommand) {
  ExecuteCommand({"SET", "mykey", "10"});
  EXPECT_EQ(GetReply(), "+OK\r\n");

  ExecuteCommand({"DECR", "mykey"});
  EXPECT_EQ(GetReply(), ":9\r\n");

  ExecuteCommand({"DECR", "mykey"});
  EXPECT_EQ(GetReply(), ":8\r\n");

  ExecuteCommand({"GET", "mykey"});
  EXPECT_EQ(GetReply(), "$1\r\n8\r\n");
}

TEST_F(CommandTest, IncrWithInvalidValue) {
  ExecuteCommand({"SET", "invalid_key", "not_an_integer"});
  EXPECT_EQ(GetReply(), "+OK\r\n");

  ExecuteCommand({"INCR", "invalid_key"});
  EXPECT_EQ(GetReply(), "-ERR value is not an integer\r\n");
}

TEST_F(CommandTest, DecrWithInvalidValue) {
  ExecuteCommand({"SET", "invalid_key", "not_an_integer"});
  EXPECT_EQ(GetReply(), "+OK\r\n");

  ExecuteCommand({"DECR", "invalid_key"});
  EXPECT_EQ(GetReply(), "-ERR value is not an integer\r\n");
}
