#include "mock_handler.h"
#include "stdio.h"
#include "stdlib.h"

static void mock_begin_array_handler(struct CommandHandler *ch, int64_t len) {}

static void mock_end_array_handler(struct CommandHandler *ch) {}

static void mock_begin_bulk_string_handler(struct CommandHandler *ch, int64_t len) {}

static void mock_end_bulk_string_handler(struct CommandHandler *ch) {}

static void mock_chars_handler(struct CommandHandler *ch, const char *begin, const char *end) {}

// to be used for unimplemented operations
static void unimplemented() { perror("unimplemented"); }

Handler *create_mock_handler() {
  Handler *handler = malloc(sizeof(Handler));
  if (!handler) {
    perror("Failed to allocate Handler");
    exit(EXIT_FAILURE);
  }

  handler->begin_array = mock_begin_array_handler;
  handler->end_array = mock_end_array_handler;
  handler->begin_bulk_string = mock_begin_bulk_string_handler;
  handler->end_bulk_string = mock_end_bulk_string_handler;
  handler->chars = mock_chars_handler;

  // Initialize unused handlers to NULL or dummy functions
  handler->begin_simple_string = mock_function;
  handler->end_simple_string = mock_function;
  handler->begin_error = mock_function;
  handler->end_error = mock_function;
  handler->begin_integer = mock_function;
  handler->end_integer = mock_function;

  return handler;
}

void destroy_mock_handler(Handler *handler) { free(handler); }

CommandHandler *create_mock_command_handler() {
  CommandHandler *mock_ch = malloc(sizeof(CommandHandler));
  return mock_ch;
}
void destroy_mock_command_handler(CommandHandler *mock_ch) { free(mock_ch); }