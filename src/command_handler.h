#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "client.h"
#include <stddef.h> // For size_t
#include <stdint.h> // For int64_t

struct Handler;

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
} CommandHandler;

CommandHandler *create_command_handler(struct Client *client, size_t initial_buf_size,
                                       size_t initial_arg_capacity);
void begin_array_handler(CommandHandler *ch, int64_t len);
void end_array_handler(CommandHandler *ch);
void begin_bulk_string_handler(CommandHandler *ch, int64_t len);
void end_bulk_string_handler(CommandHandler *ch);
void chars_handler(CommandHandler *ch, const char *begin, const char *end);
void destroy_command_handler(CommandHandler *ch);
void destory_handler(struct Handler *h);

#ifdef __cplusplus
}
#endif

#endif // COMMAND_HANDLER_H