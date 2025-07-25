#include <gtest/gtest.h>
extern "C" {
#include "../src/client.h"
#include "../src/command_handler.h"
#include "../src/database.h"
#include "../src/server_config.h"
}

redis_db_t *db;

class CommandTest : public ::testing::Test {
protected:
  void SetUp() override {
    db = redis_db_create();
    client = create_client(-1); // -1 as we don't need a real socket for tests
    select_client_db(client, db);
    ch = create_command_handler(client, 1024, 10);
  }

  void TearDown() override {
    destroy_command_handler(ch);
    destroy_client(client);
    redis_db_destroy(db);
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

TEST_F(CommandTest, LpushCommand) {
  ExecuteCommand({"LPUSH", "mylist", "car"});
  EXPECT_EQ(GetReply(), ":1\r\n");

  ExecuteCommand({"LPUSH", "mylist", "dog"});
  EXPECT_EQ(GetReply(), ":2\r\n");
}

TEST_F(CommandTest, LpushCommand_MultipleItems) {
  ExecuteCommand({"LPUSH", "mylist", "car", "dog", "house"});
  EXPECT_EQ(GetReply(), ":3\r\n");
}

TEST_F(CommandTest, RpushCommand_MultipleItems) {
  ExecuteCommand({"LPUSH", "mylist", "car", "dog", "house"});
  EXPECT_EQ(GetReply(), ":3\r\n");
}

TEST_F(CommandTest, RpushCommand) {
  ExecuteCommand({"RPUSH", "mylist", "car"});
  EXPECT_EQ(GetReply(), ":1\r\n");

  ExecuteCommand({"RPUSH", "mylist", "dog"});
  EXPECT_EQ(GetReply(), ":2\r\n");
}

TEST_F(CommandTest, GetStringOnList_ShouldReturnError) {
  ExecuteCommand({"LPUSH", "mylist", "car"});
  EXPECT_EQ(GetReply(), ":1\r\n");

  ExecuteCommand({"GET", "mylist"});
  EXPECT_EQ(GetReply(), "-ERR Operation against a key holding the wrong kind of value\r\n");
}

TEST_F(CommandTest, LpushRpushOnStringValue_ShouldReturnError) {
  ExecuteCommand({"SET", "mylist", "x"});
  EXPECT_EQ(GetReply(), "+OK\r\n");

  ExecuteCommand({"LPUSH", "mylist", "car"});
  EXPECT_EQ(GetReply(), "-ERR Operation against a key holding the wrong kind of value\r\n");

  ExecuteCommand({"RPUSH", "mylist", "car"});
  EXPECT_EQ(GetReply(), "-ERR Operation against a key holding the wrong kind of value\r\n");
}

TEST_F(CommandTest, LpushAndDelete_ShouldDelete) {
  ExecuteCommand({"LPUSH", "mylist", "car"});
  EXPECT_EQ(GetReply(), ":1\r\n");

  ExecuteCommand({"DEL", "mylist", "car"});
  EXPECT_EQ(GetReply(), "+OK\r\n");
}

TEST_F(CommandTest, LrangeValidRange) {
  ExecuteCommand({"LPUSH", "mylist", "item1"});
  EXPECT_EQ(GetReply(), ":1\r\n");
  ExecuteCommand({"LPUSH", "mylist", "item2"});
  EXPECT_EQ(GetReply(), ":2\r\n");
  ExecuteCommand({"LPUSH", "mylist", "item3"});
  EXPECT_EQ(GetReply(), ":3\r\n");

  ExecuteCommand({"LRANGE", "mylist", "0", "2"});
  EXPECT_EQ(GetReply(), "*3\r\n$5\r\nitem3\r\n$5\r\nitem2\r\n$5\r\nitem1\r\n");
}

TEST_F(CommandTest, LrangePartialRange) {
  ExecuteCommand({"LPUSH", "mylist", "item1"});
  EXPECT_EQ(GetReply(), ":1\r\n");
  ExecuteCommand({"LPUSH", "mylist", "item2"});
  EXPECT_EQ(GetReply(), ":2\r\n");
  ExecuteCommand({"LPUSH", "mylist", "item3"});
  EXPECT_EQ(GetReply(), ":3\r\n");

  ExecuteCommand({"LRANGE", "mylist", "1", "2"});

  EXPECT_EQ(GetReply(), "*2\r\n$5\r\nitem2\r\n$5\r\nitem1\r\n");
}

TEST_F(CommandTest, LrangeInvalidRange) {
  ExecuteCommand({"LPUSH", "mylist", "item1"});
  EXPECT_EQ(GetReply(), ":1\r\n");
  ExecuteCommand({"LPUSH", "mylist", "item2"});
  EXPECT_EQ(GetReply(), ":2\r\n");
  ExecuteCommand({"LPUSH", "mylist", "item3"});
  EXPECT_EQ(GetReply(), ":3\r\n");

  // test an invalid range (out of bounds)
  ExecuteCommand({"LRANGE", "mylist", "3", "5"});
  EXPECT_EQ(GetReply(), "*0\r\n");

  // test with start greater than end
  ExecuteCommand({"LRANGE", "mylist", "2", "1"});
  EXPECT_EQ(GetReply(), "*0\r\n");
}

TEST_F(CommandTest, LrangeSingleElementRange) {
  ExecuteCommand({"LPUSH", "mylist", "item1"});
  EXPECT_EQ(GetReply(), ":1\r\n");

  ExecuteCommand({"LRANGE", "mylist", "0", "0"});
  EXPECT_EQ(GetReply(), "*1\r\n$5\r\nitem1\r\n");
}

TEST_F(CommandTest, LrangeNegativeIndices) {
  ExecuteCommand({"LPUSH", "mylist", "item1"});
  EXPECT_EQ(GetReply(), ":1\r\n");
  ExecuteCommand({"LPUSH", "mylist", "item2"});
  EXPECT_EQ(GetReply(), ":2\r\n");
  ExecuteCommand({"LPUSH", "mylist", "item3"});
  EXPECT_EQ(GetReply(), ":3\r\n");

  ExecuteCommand({"LRANGE", "mylist", "-3", "-1"});
  EXPECT_EQ(GetReply(), "*3\r\n$5\r\nitem3\r\n$5\r\nitem2\r\n$5\r\nitem1\r\n");

  ExecuteCommand({"LRANGE", "mylist", "-3", "1"});
  EXPECT_EQ(GetReply(), "*2\r\n$5\r\nitem3\r\n$5\r\nitem2\r\n");
}

TEST_F(CommandTest, LrangeKeyNotFound) {
  ExecuteCommand({"LRANGE", "non_existent_list", "0", "2"});

  EXPECT_EQ(GetReply(), "*0\r\n");
}

TEST_F(CommandTest, GetConfig) {
  strcpy(g_server_config.dir, "testdir");
  strcpy(g_server_config.dbfilename, "testdb.rdb");

  ExecuteCommand({"CONFIG", "GET", "dir"});
  EXPECT_EQ(GetReply(), "*2\r\n$3\r\ndir\r\n$" + std::to_string(strlen(g_server_config.dir)) +
                            "\r\n" + std::string(g_server_config.dir) + "\r\n");

  ExecuteCommand({"CONFIG", "GET", "dbfilename"});
  EXPECT_EQ(GetReply(), "*2\r\n$10\r\ndbfilename\r\n$" +
                            std::to_string(strlen(g_server_config.dbfilename)) + "\r\n" +
                            std::string(g_server_config.dbfilename) + "\r\n");
}