#ifndef HANDLER_H
#define HANDLER_H

#include <stdint.h>

struct CommandHandler;

typedef struct Handler {
  void (*begin_array)(struct CommandHandler *ch, int64_t len);
  void (*end_array)(struct CommandHandler *ch);
  void (*begin_bulk_string)(struct CommandHandler *ch, int64_t len);
  void (*end_bulk_string)(struct CommandHandler *ch);
  void (*chars)(struct CommandHandler *ch, const char *begin, const char *end);
  void (*begin_simple_string)(struct CommandHandler *ch);
  void (*end_simple_string)(struct CommandHandler *ch);
  void (*begin_error)(struct CommandHandler *ch);
  void (*end_error)(struct CommandHandler *ch);
  void (*begin_integer)(struct CommandHandler *ch);
  void (*end_integer)(struct CommandHandler *ch);
} Handler;

Handler *create_handler();
void destroy_handler(Handler *handler);

#endif // HANDLER_H