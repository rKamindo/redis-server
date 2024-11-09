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
