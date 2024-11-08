#ifndef RESP_H
#define RESP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "command_handler.h"
#include "handler.h"
#include "ring_buffer.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// forward declarations:
struct Parser;
struct Handler;

// writes the beginning of a simple string to the ring buffer
void write_begin_simple_string(ring_buffer rb);

// writes the end of a simple string to the ring buffer
void write_end_simple_string(ring_buffer rb);

// writes the beginning of an error message to the ring buffer
void write_begin_error(ring_buffer rb);

// writes the end of an error message to the ring buffer
void write_end_error(ring_buffer rb);

// writes the beginning of an integer to the ring buffer
void write_begin_integer(ring_buffer rb);

// writes the end of an integer to the ring buffer
void write_end_integer(ring_buffer rb);

// writes the beginning of a bulk string to the ring buffer with its length
void write_begin_bulk_string(ring_buffer rb, int64_t len);

// writes the end of a bulk string to the ring buffer
void write_end_bulk_string(ring_buffer rb);

// writes the beginning of an array to the ring buffer with its length
void write_begin_array(ring_buffer rb, int64_t len);

// writes the end of an array to the ring buffer
void write_end_array(ring_buffer rb);

// writes a string to the ring buffer
void write_chars(ring_buffer rb, const char *str);

typedef enum {
  STATE_INITIAL_TERMINAL,
  STATE_INITIAL,
  STATE_SIMPLE,
  STATE_ARRAY_LENGTH,
  STATE_BULK_STRING_LENGTH,
  STATE_BULK_STRING,
  STATE_ARRAY,
  STATE_ERROR,
  STATE_INTEGER,
  STATE_INLINE_COMMAND
} ParserState;

typedef struct {
  bool keep_going;
  const char *new_begin;
} ParseResult;

// Function pointer types
typedef ParseResult (*ParseFunc)(struct Parser *, const char *, const char *);

typedef struct {
  ParserState type;
  int64_t length;
  ParseFunc parse;
  void (*end_callback)(struct CommandHandler *);
} StateInfo;

// Constants
#define MAX_STACK_DEPTH 32

// Main Parser struct
typedef struct Parser {
  struct Handler *handler;
  struct CommandHandler *command_handler;
  StateInfo stack[MAX_STACK_DEPTH];
  int stack_top;
} Parser;

void parser_init(Parser *parser, struct Handler *handler, struct CommandHandler *command_handler);
const char *parser_parse(Parser *parser, const char *begin, const char *end);
void destroy_parser(Parser *parser);

#ifdef __cplusplus
}
#endif

#endif // RESP_H