#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "command_handler.h"
#include "commands.h"
#include "replication.h"

Handler *g_handler = NULL;

CommandType get_command_type(char *command) {
  if (strcmp(command, "PING") == 0)
    return CMD_PING;
  else if (strcmp(command, "ECHO") == 0)
    return CMD_ECHO;
  else if (strcmp(command, "SET") == 0)
    return CMD_SET;
  else if (strcmp(command, "GET") == 0)
    return CMD_GET;
  else if (strcmp(command, "EXIST") == 0)
    return CMD_EXIST;
  else if (strcmp(command, "DEL") == 0)
    return CMD_DEL;
  else if (strcmp(command, "INCR") == 0)
    return CMD_INCR;
  else if (strcmp(command, "DECR") == 0)
    return CMD_DECR;
  else if (strcmp(command, "LPUSH") == 0)
    return CMD_LPUSH;
  else if (strcmp(command, "RPUSH") == 0)
    return CMD_RPUSH;
  else if (strcmp(command, "LRANGE") == 0)
    return CMD_LRANGE;
  else if (strcmp(command, "CONFIG") == 0)
    return CMD_CONFIG;
  else if (strcmp(command, "SAVE") == 0)
    return CMD_SAVE;
  else if (strcmp(command, "DBSIZE") == 0)
    return CMD_DBSIZE;
  else if (strcmp(command, "INFO") == 0)
    return CMD_INFO;
  else if (strcmp(command, "REPLCONF") == 0) {
    return CMD_REPLCONF;
  } else if (strcmp(command, "PSYNC") == 0) {
    return CMD_PSYNC;
  } else
    return CMD_UNKNOWN;
}

void handle_command(CommandHandler *ch) {
  ch->client->should_propogate_command = false;

  CommandType command_type = get_command_type(ch->args[0]);

  switch (command_type) {
  case CMD_PING:
    handle_ping(ch);
    break;
  case CMD_ECHO:
    handle_echo(ch);
    break;
  case CMD_SET:
    handle_set(ch);
    break;
  case CMD_GET:
    handle_get(ch);
    break;
  case CMD_EXIST:
    handle_exist(ch);
    break;
  case CMD_DEL:
    handle_delete(ch);
    break;
  case CMD_INCR:
    handle_incr(ch);
    break;
  case CMD_DECR:
    handle_decr(ch);
    break;
  case CMD_LPUSH:
    handle_lpush(ch);
    break;
  case CMD_RPUSH:
    handle_rpush(ch);
    break;
  case CMD_LRANGE:
    handle_lrange(ch);
    break;
  case CMD_CONFIG:
    handle_config(ch);
    break;
  case CMD_SAVE:
    handle_save(ch);
    break;
  case CMD_DBSIZE:
    handle_dbsize(ch);
    break;
  case CMD_INFO:
    handle_info(ch);
    break;
  case CMD_REPLCONF:
    handle_replconf(ch);
    break;
  case CMD_PSYNC:
    handle_psync(ch);
    break;
  default:
    add_error_reply(ch->client, "ERR unknown command");
    break;
  }
  // if it is a write command
  // for now propogate set
  if (command_type == CMD_SET) {
    ch->client->should_propogate_command = true;
  }
}

CommandHandler *create_command_handler(Client *client, size_t initial_buf_size,
                                       size_t initial_arg_capacity) {
  CommandHandler *ch = malloc(sizeof(CommandHandler));

  ch->client = client;

  if (!ch) {
    perror("failed to allocate CommandHandler");
    exit(EXIT_FAILURE);
  }

  ch->buf = malloc(initial_buf_size);
  ch->args = malloc(sizeof(char *) * initial_arg_capacity);
  ch->ends = malloc(sizeof(size_t) * initial_arg_capacity);

  if (!ch->buf || !ch->args || !ch->ends) {
    perror("failed to allocate CommandHandler buffers");
    exit(EXIT_FAILURE);
  }

  ch->buf_size = initial_buf_size;
  ch->buf_used = 0;
  ch->arg_capacity = initial_arg_capacity;
  ch->arg_count = 0;
  ch->ends_size = 0;
  ch->ends_capacity = initial_arg_capacity;

  return ch;
}

// Command handler methods
void begin_array_handler(CommandHandler *ch, int64_t len) {
  ch->buf_used = 0;
  ch->arg_count = 0;
  ch->ends_size = 0;

  // ensure args array has enough capacity
  if (len > ch->arg_capacity) {
    ch->arg_capacity = len;
    ch->args = realloc(ch->args, sizeof(char *) * len);
    if (ch->args == NULL) {
      perror("memory realloc failed for args array");
      exit(EXIT_FAILURE);
    }
  }

  // ensure ends array has enough capacity
  if (len > ch->ends_capacity) {
    ch->ends_capacity = len;
    ch->ends = realloc(ch->ends, sizeof(size_t) * len);
    if (ch->ends == NULL) {
      perror("memory realloc failed for ends array");
      exit(EXIT_FAILURE);
    }
  }
}

void end_array_handler(CommandHandler *ch) {
  // split buffer into arguments
  char *begin = ch->buf;
  for (size_t i = 0; i < ch->ends_size; i++) {
    size_t len = ch->ends[i] - (begin - ch->buf);
    ch->args[ch->arg_count] = malloc(len + 1);
    if (ch->args[ch->arg_count] == NULL) {
      perror("failure to allocate memory for arg");
      return;
    }

    memcpy(ch->args[ch->arg_count], begin, len);
    ch->args[ch->arg_count][len] = '\0';

    ch->arg_count++;
    begin = ch->buf + ch->ends[i];
  }

  // execute the command
  handle_command(ch);

  // free memory allocated for arguments
  for (int i = 0; i < ch->arg_count; i++) {
    free(ch->args[i]);
  }
}

void begin_bulk_string_handler(CommandHandler *ch, int64_t len) {
  // ensure buffer has enough space for current content plus new string
  size_t required_size = ch->buf_used + len;
  if (required_size > ch->buf_size) {
    ch->buf_size = required_size;
    ch->buf = realloc(ch->buf, ch->buf_size);
    if (ch->buf == NULL) {
      perror("memory realloc failed for resizing buffer");
      exit(EXIT_FAILURE);
    }
  }
}

void end_bulk_string_handler(CommandHandler *ch) {
  // update ends array with end of the current bulk string
  ch->ends[ch->ends_size] = ch->buf_used;
  ch->ends_size++;
}

void chars_handler(CommandHandler *ch, const char *begin, const char *end) {
  size_t len = end - begin;
  memcpy(ch->buf + ch->buf_used, begin, len);
  ch->buf_used += len;
}

void begin_simple_string_handler(CommandHandler *ch) { ch->buf_used = 0; }

void end_simple_string_handler(CommandHandler *ch) {
  if (ch->buf_used < ch->buf_size) {
    ch->buf[ch->buf_used] = '\0';
  } else {
    fprintf(stderr, "Error: Simple string buffer too small for null terminator.\n");
    ch->client->repl_client_state = REPL_STATE_ERROR;
    return;
  }

  printf("received: %s\n", ch->buf);
  handle_simple_string_reply(ch);
}

void unimplemented(CommandHandler *ch) { perror("unimplemented"); }

// create handler interface for the parser
Handler *create_handler() {
  Handler *handler = malloc(sizeof(Handler));
  if (!handler) {
    perror("Failed to allocate Handler");
    exit(EXIT_FAILURE);
  }

  // Setup the handler interface functions
  handler->begin_array = begin_array_handler;
  handler->end_array = end_array_handler;
  handler->begin_bulk_string = begin_bulk_string_handler;
  handler->end_bulk_string = end_bulk_string_handler;
  handler->chars = chars_handler;

  // set unused handler functions to an unimplemented function
  handler->begin_simple_string = begin_simple_string_handler;
  handler->end_simple_string = end_simple_string_handler;
  handler->begin_error = unimplemented;
  handler->end_error = unimplemented;
  handler->begin_integer = unimplemented;
  handler->end_integer = unimplemented;

  // assign newly created instance to global handler
  g_handler = handler;

  return handler;
}

void destroy_command_handler(CommandHandler *ch) {
  if (ch) {
    free(ch->args);
    free(ch->buf);
    free(ch->ends);
    free(ch);
  }
}

void destroy_handler(Handler *handler) {
  if (handler) {
    free(handler);
  }
}
