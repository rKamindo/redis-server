#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "client.h"
#include <stddef.h>
#include <stdint.h>

typedef enum {
  CMD_PING,
  CMD_ECHO,
  CMD_SET,
  CMD_GET,
  CMD_EXIST,
  CMD_DEL,
  CMD_INCR,
  CMD_DECR,
  CMD_LPUSH,
  CMD_RPUSH,
  CMD_LRANGE,
  CMD_CONFIG,
  CMD_SAVE,
  CMD_DBSIZE,
  CMD_INFO,
  CMD_UNKNOWN,
  CMD_REPLCONF,
  CMD_PSYNC
} CommandType;

typedef struct CommandHandler {
  char *buf;
  size_t buf_size;
  size_t buf_used;
  char **args;
  size_t arg_capacity;
  size_t arg_count;
  size_t *ends;
  size_t ends_size;
  size_t ends_capacity;
  struct Client *client;
  bool should_respond;
} CommandHandler;

CommandHandler *create_command_handler(struct Client *client, size_t initial_buf_size,
                                       size_t initial_arg_capacity);
void handle_command(CommandHandler *ch);
void destroy_command_handler(CommandHandler *ch);
void begin_array_handler(CommandHandler *ch, int64_t len);
void end_array_handler(CommandHandler *ch);
void begin_bulk_string_handler(CommandHandler *ch, int64_t len);
void end_bulk_string_handler(CommandHandler *ch);
void chars_handler(CommandHandler *ch, const char *begin, const char *end);

#ifdef __cplusplus
}
#endif

#endif // COMMAND_HANDLER_H