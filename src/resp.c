#include "resp.h"
#include "ring_buffer.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char cr = '\r';
const char lf = '\n';
const char crlf[2] = "\r\n";

void write_begin_simple_string(ring_buffer rb) {
  // check if there is enough writable space in the ring buffer
  char *output_buf;
  size_t writable_len;

  // get writable space in the ring buffer
  if (rb_writable(rb, &output_buf, &writable_len) != 0 || writable_len < 1) {
    perror("not enough space in ring buffer to write simple string start.\n");
    return;
  }

  output_buf[0] = '+';
  rb_write(rb, 1);
}

void write_end_simple_string(ring_buffer rb) {
  char *output_buf;
  size_t writable_len;

  if (rb_writable(rb, &output_buf, &writable_len) != 0 || writable_len < 2) {
    perror("not enough space in ring buffer to write end simple string.\n");
    return;
  }

  memcpy(output_buf, crlf, 2);
  rb_write(rb, 2);
}

void write_begin_error(ring_buffer rb) {
  // check if there is enough writable space in the ring buffer
  char *output_buf;
  size_t writable_len;

  // get writable space in the ring buffer
  if (rb_writable(rb, &output_buf, &writable_len) != 0 || writable_len < 1) {
    perror("not enough space in ring buffer to write begin error.\n");
    return;
  }

  output_buf[0] = '-';
  rb_write(rb, 1);
}

void write_end_error(ring_buffer rb) {
  char *output_buf;
  size_t writable_len;

  if (rb_writable(rb, &output_buf, &writable_len) != 0 || writable_len < 2) {
    perror("not enough space in ring buffer to write end error.\n");
    return;
  }

  memcpy(output_buf, crlf, 2);
  rb_write(rb, 2);
}

void write_begin_integer(ring_buffer rb) {
  // check if there is enough writable space in the ring buffer
  char *output_buf;
  size_t writable_len;

  // get writable space in the ring buffer
  if (rb_writable(rb, &output_buf, &writable_len) != 0 || writable_len < 1) {
    perror("not enough space in ring buffer to write begin integer.\n");
    return;
  }

  output_buf[0] = ':';
  rb_write(rb, 1);
}

void write_end_integer(ring_buffer rb) {
  char *output_buf;
  size_t writable_len;

  if (rb_writable(rb, &output_buf, &writable_len) != 0 || writable_len < 2) {
    perror("not enough space in ring buffer to write end integer.\n");
    return;
  }

  memcpy(output_buf, crlf, 2);
  rb_write(rb, 2);
}

void write_begin_bulk_string(ring_buffer rb, int64_t len) {
  char *output_buf;
  size_t writable_len;

  int needed_length = snprintf(NULL, 0, "$%ld", len);
  if (rb_writable(rb, &output_buf, &writable_len) != 0 ||
      writable_len < needed_length + (len != -1 ? 2 : 0)) {
    perror("not enough space in ring buffer to write begin builk string.");
    return;
  }

  // write the length prefixed by $
  snprintf(output_buf, writable_len, "$%ld", len);

  rb_write(rb, needed_length);
  if (len != -1) {
    memcpy(output_buf + needed_length, crlf, 2);
    rb_write(rb, 2);
  }
}

void write_end_bulk_string(ring_buffer rb) {
  char *output_buf;
  size_t writable_len;

  if (rb_writable(rb, &output_buf, &writable_len) != 0 || writable_len < 2) {
    perror("not enough space in ring buffer to write end integer.\n");
    return;
  }

  memcpy(output_buf, crlf, 2);
  rb_write(rb, 2);
}

void write_begin_array(ring_buffer rb, int64_t len) {
  char *output_buf;
  size_t writable_len;

  // calculate the needed length for the array header
  int needed_length = snprintf(NULL, 0, "*%ld\r\n", len);

  // check if there is enough writable space in the ring buffer
  if (rb_writable(rb, &output_buf, &writable_len) != 0 || writable_len < needed_length) {
    perror("not enough space in ring buffer to write begin array.\n");
    return;
  }

  // write the length prefixed by '*'
  snprintf(output_buf, writable_len, "*%ld\r\n", len);

  // update the write index for the length
  rb_write(rb, needed_length);
}

void write_end_array(ring_buffer rb) {
  // no specific end marker for arrays
  return;
}

void write_chars(ring_buffer rb, const char *str) {
  char *output_buf;
  size_t writable_len;

  size_t str_length = strlen(str);

  if (rb_writable(rb, &output_buf, &writable_len) != 0 || writable_len < str_length) {
    perror("Not enough space in ring buffer to write string.\n");
  }

  memcpy(output_buf, str, str_length);

  rb_write(rb, str_length);
}

ParseResult parse_initial(Parser *parser, const char *begin, const char *end);
ParseResult parse_simple(Parser *parser, const char *begin, const char *end);
ParseResult parse_length(Parser *parser, const char *begin, const char *end);
ParseResult parse_bulk_string(Parser *parser, const char *begin, const char *end);
ParseResult parse_array(Parser *parser, const char *begin, const char *end);
ParseResult parse_inline_command(Parser *parser, const char *begin, const char *end);

void parser_init(Parser *parser, Handler *handler, CommandHandler *command_handler) {
  parser->handler = handler;
  parser->command_handler = command_handler;
  parser->stack_top = 0;
  parser->stack[0] = (StateInfo){STATE_INITIAL_TERMINAL, 0, parse_initial};
}

void push_state(Parser *parser, ParserState state, int64_t length, ParseFunc parse_func,
                void (*end_callback)(CommandHandler *)) {
  if (parser->stack_top < MAX_STACK_DEPTH - 1) {
    parser->stack_top++;
    parser->stack[parser->stack_top] = (StateInfo){state, length, parse_func, end_callback};
  }
}

void pop_state(Parser *parser) {
  if (parser->stack_top > 0) {
    parser->stack_top--;
  }
}

const char *parser_parse(Parser *parser, const char *begin, const char *end) {
  bool keep_going = true;
  while (keep_going) {
    ParseResult result = parser->stack[parser->stack_top].parse(parser, begin, end);
    keep_going = result.keep_going;
    begin = result.new_begin;
  }
  return begin;
}

ParseResult parse_initial(Parser *parser, const char *begin, const char *end) {
  if (begin == end) return (ParseResult){false, begin};

  // check if this is a non-terminal initial state (i.e., not STATE_INITIAL_TERMINAL)
  ParserState current_state = parser->stack[parser->stack_top].type;
  if (current_state == STATE_INITIAL) {
    // if it is a non-terminal initial state
    // pop it off the stack before processing the next element
    pop_state(parser);
  }

  switch (*begin) {
  case '+':
    parser->handler->begin_simple_string(parser->command_handler);
    push_state(parser, STATE_SIMPLE, 0, parse_simple, parser->handler->end_simple_string);
    break;
  case '-':
    parser->handler->begin_error(parser->command_handler);
    push_state(parser, STATE_SIMPLE, 0, parse_simple, parser->handler->end_error);
    break;
  case ':':
    push_state(parser, STATE_SIMPLE, 0, parse_simple, parser->handler->end_integer);
    parser->handler->begin_integer(parser->command_handler);
    break;
  case '$':
    push_state(parser, STATE_BULK_STRING_LENGTH, 0, parse_length, parser->handler->end_bulk_string);
    // don't call begin_bulk_string yet, we need the length first
    break;
  case '*':
    push_state(parser, STATE_ARRAY_LENGTH, 0, parse_length, parser->handler->end_array);
    // don't call begin_array yet, we need the length first
    break;

  default:
    push_state(parser, STATE_INLINE_COMMAND, 0, parse_inline_command, NULL);
    return (ParseResult){true, begin};
  }
  return (ParseResult){true, begin + 1};
}

ParseResult parse_simple(Parser *parser, const char *begin, const char *end) {
  if (begin == end) return (ParseResult){false, begin};

  const char *pos = begin;
  while (pos < end && *pos != cr) {
    pos++;
  }

  // process the characters up to CR
  parser->handler->chars(parser->command_handler, begin, pos);

  if (end - pos < 2) {
    return (ParseResult){false, pos};
  } else if (pos[1] == lf) { // found end of the message
    parser->stack[parser->stack_top].end_callback(parser->command_handler);
    pop_state(parser);                   // remove current state
    return (ParseResult){true, pos + 2}; // move past \r\n
  } else {
    // handle error, carraige return without newline
    return (ParseResult){false, pos};
  }
}

ParseResult parse_length(Parser *parser, const char *begin, const char *end) {
  if (begin == end) return (ParseResult){false, begin};
  // "*4\r\n$4\r\necho\r\n$11\r\nhello world\r\n$4\r\necho\r\n$2\r\nhi\r\n”
  // parse the length
  const char *pos = begin;
  int64_t length = 0;
  int sign = 1;

  // check for negative length
  if (*pos == '-') {
    sign = -1;
    pos++;
  }

  while (pos < end - 1) {
    if (pos[0] == cr && pos[1] == lf) {
      // found CRLF
      if (pos == begin || (sign == -1 && pos == begin + 1)) {
        // empty of just a minus sign
        return (ParseResult){false, begin};
      }

      length *= sign;

      if (parser->stack[parser->stack_top].type == STATE_ARRAY_LENGTH) {
        // remove the length state
        pop_state(parser);
        parser->handler->begin_array(parser->command_handler, length);
        push_state(parser, STATE_ARRAY, length, parse_array, parser->handler->end_array);
      } else if (parser->stack[parser->stack_top].type == STATE_BULK_STRING_LENGTH) {
        // remove the length state
        pop_state(parser);
        parser->handler->begin_bulk_string(parser->command_handler, length);
        push_state(parser, STATE_BULK_STRING, length, parse_bulk_string,
                   parser->handler->end_bulk_string);
      }

      return (ParseResult){true, pos + 2}; // move past CRLF
    } else if (*pos < '0' || *pos > '9') {
      // invalid character
      return (ParseResult){false, begin};
    }
    length = length * 10 + (*pos - '0');
    pos++;
  }

  // reached end without finding CRLF
  return (ParseResult){false, begin};
}

// returns the minimum of two values
int min(int64_t a, int64_t b) { return a <= b ? a : b; }

ParseResult parse_bulk_string(Parser *parser, const char *begin, const char *end) {
  int64_t length = parser->stack[parser->stack_top].length;

  if (length == -1) {
    parser->handler->end_bulk_string(parser->command_handler);
    pop_state(parser);
    return (ParseResult){true, begin};
  }

  if (begin == end) {
    return (ParseResult){false, begin};
  }

  int64_t input_length = end - begin;
  int64_t output_length = min(length, input_length);
  parser->handler->chars(parser->command_handler, begin, begin + output_length);
  length -= output_length;

  if (length == 0 && input_length >= output_length + 2) {
    parser->handler->end_bulk_string(parser->command_handler);
    pop_state(parser);
    return (ParseResult){true, begin + output_length + 2};
  } else {
    return (ParseResult){false, begin + output_length};
  }
}

ParseResult parse_array(Parser *parser, const char *begin, const char *end) {
  int64_t length = parser->stack[parser->stack_top].length;

  if (length == 0 || length == -1) {
    parser->handler->end_array(parser->command_handler);
    pop_state(parser);
    return (ParseResult){true, begin};
  }
  parser->stack[parser->stack_top].length--;
  push_state(parser, STATE_INITIAL, 0, parse_initial, NULL);
  return (ParseResult){true, begin};
}

size_t count_tokens(const char *str) {
  size_t token_count = 0;
  int in_token = 0; // Flag to track if we are inside a token

  while (*str) {
    if (*str == ' ') {
      if (in_token) {
        in_token = 0; // Exiting a token
      }
    } else {
      if (!in_token) {
        token_count++; // Found a new token
        in_token = 1;  // Entering a token
      }
    }
    str++;
  }

  return token_count;
}

ParseResult parse_inline_command(Parser *parser, const char *begin, const char *end) {
  if (begin == end) return (ParseResult){false, begin};

  const char *pos = begin;
  while (pos < end && *pos != '\r') {
    pos++;
  }

  if (pos != end) {

    // process characters up to CR
    const char *token_start = begin;

    size_t command_length = pos - begin; // length of the command

    char command_copy[command_length + 1];
    strncpy(command_copy, begin, command_length);
    command_copy[command_length] = '\0';

    const char *delimiter = " ";

    size_t token_count = count_tokens(begin); // count tokens

    char *token = strtok(command_copy, delimiter);

    parser->handler->begin_array(parser->command_handler, token_count);

    while (token != NULL) {
      size_t token_length = strlen(token);

      parser->handler->begin_bulk_string(parser->command_handler, token_length);
      parser->handler->chars(parser->command_handler, token, token + token_length);
      parser->handler->end_bulk_string(parser->command_handler);

      token = strtok(NULL, delimiter); // get the next token
    }

    parser->handler->end_array(parser->command_handler);
    pop_state(parser);
    return (ParseResult){true, pos + 2};
  } else {
    return (ParseResult){false, begin};
  }
}

void destroy_parser(Parser *parser) { free(parser); }