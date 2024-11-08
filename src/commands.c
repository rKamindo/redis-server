#include "commands.h"
#include "command_handler.h"
#include "database.h"
#include "sys/time.h"
#include <errno.h>
#include <stddef.h>

void add_simple_string_reply(Client *client, const char *str) {
  write_begin_simple_string(client->output_buffer);
  write_chars(client->output_buffer, str);
  write_end_simple_string(client->output_buffer);
}

void add_bulk_string_reply(Client *client, const char *str) {
  // Assuming you have a function that calculates the length of the string
  int64_t len = strlen(str);
  write_begin_bulk_string(client->output_buffer, len);
  write_chars(client->output_buffer, str);
  write_end_bulk_string(client->output_buffer);
}

void add_null_reply(Client *client) {
  write_begin_bulk_string(client->output_buffer, -1); // Indicate that this is a null bulk string
  // No actual data to write since it's null
  write_end_bulk_string(client->output_buffer);
}

void add_error_reply(Client *client, const char *str) {
  write_begin_error(client->output_buffer);
  write_chars(client->output_buffer, str);
  write_end_error(client->output_buffer);
}

#define ERR_SYNTAX -1
#define ERR_OTHER -2

int parse_integer(const char *str, long *result) {
  char *endptr;
  errno = 0;
  long value = strtol(str, &endptr, 10);

  // check if the conversion was successful and if the value is within range
  if (endptr == str || *endptr != '\0' || errno == ERANGE || value < 0) {
    return ERR_OTHER; // return error code for non-integer or out of range
  }

  *result = value;
  return 0;
}

/*
Parses the options sent to a SET command, returns 0 if successful, -1 if there is a syntax error
*/
int parse_set_options(char **args, int args_count, SetOptions *options) {

  // iterate through the arguments provided to the SET command
  for (int i = 0; i < args_count; i++) {
    // check for the "NX" option (only set if the key does not exist)
    if (strcmp(args[i], "NX") == 0) {
      if (options->xx) return ERR_SYNTAX; // NX and XX are mutually exclusive
      options->nx = 1;
      // check for the "XX" option (only set if the key already exists)
    } else if (strcmp(args[i], "XX") == 0) {
      if (options->nx) return ERR_SYNTAX; // NX and XX are mutually exclusive
      options->xx = 1;
    } else if (strcmp(args[i], "GET") == 0) {
      options->get = 1;
      // check for the "KEEPTTL" option (keep the time-to-live of the key)
    } else if (strcmp(args[i], "KEEPTTL") == 0) {
      options->keepttl = 1;
      // check for the "EX" option (set expiration time in seconds)
    } else if (strcmp(args[i], "EX") == 0 && i + 1 < args_count) {
      if (options->expiration || options->keepttl) return ERR_SYNTAX;
      char *next_arg = args[++i];
      long seconds;
      int parse_result = parse_integer(next_arg, &seconds);
      if (parse_result != 0) { // some error occured, non-integer or out of range error found
        return parse_result;
      }
      options->expiration = current_time_millis() + seconds * 1000;
    } else if (strcmp(args[i], "PX") == 0 && i + 1 < args_count) {
      if (options->expiration || options->keepttl) return ERR_SYNTAX;
      char *next_arg = args[++i];
      long milliseconds;
      int parse_result = parse_integer(next_arg, &milliseconds);
      if (parse_result != 0) { // some error occured, non-integer or out of range error found
        return parse_result;
      }
      options->expiration = current_time_millis() + milliseconds;
      // check for the "EXAT" option (set absolute expiration time in seconds since epoch)
    } else if (strcmp(args[i], "EXAT") == 0 && i + 1 < args_count) {
      if (options->expiration || options->keepttl) return ERR_SYNTAX;
      char *next_arg = args[++i];
      long seconds;
      int parse_result = parse_integer(next_arg, &seconds);
      if (parse_result != 0) { // some error occured, non-integer or out of range error found
        return parse_result;
      }
      options->expiration = seconds * 1000;
      // check for the "PXAT" option (set absolute expiration time in milliseconds since epoch)
    } else if (strcmp(args[i], "PXAT") == 0 && i + 1 < args_count) {
      if (options->expiration || options->keepttl) return ERR_SYNTAX;
      char *next_arg = args[++i];
      long milliseconds;
      int parse_result = parse_integer(next_arg, &milliseconds);
      if (parse_result != 0) { // some error occured, non-integer or out of range error found
        return parse_result;
      }
      options->expiration = milliseconds;
    }
  }
  return 0;
}

void handle_ping(CommandHandler *ch) {
  if (ch->arg_count == 1) {
    add_simple_string_reply(ch->client, "PONG");
  } else if (ch->arg_count == 2) {
    add_simple_string_reply(ch->client, ch->args[1]);
  } else {
    add_error_reply(ch->client, "ERR wrong number of arguments for 'ping' command");
  }
}

void handle_echo(CommandHandler *ch) {
  if (ch->arg_count != 2) {
    add_error_reply(ch->client, "ERR wrong number of arguments for 'echo' command");
    return;
  }
  add_simple_string_reply(ch->client, ch->args[1]);
}

void handle_set(CommandHandler *ch) {
  if (ch->arg_count < 3) {
    add_error_reply(ch->client, "ERR wrong number of arguments for 'set' command");
    return;
  }

  SetOptions options = {0};
  int parse_result = parse_set_options(ch->args + 3, ch->arg_count - 3, &options);

  switch (parse_result) {
  case ERR_SYNTAX:
    add_error_reply(ch->client, "ERR syntax error");
    return;
  case ERR_OTHER:
    add_error_reply(ch->client, "ERR value is not an integer or out of range");
    return;
  }

  RedisValue *existing_value = NULL;
  bool key_exists = false;

  // only get the existing value if we need to check conditions (NX, XX) or respond to GET option
  if (options.nx || options.xx || options.get) {
    existing_value = redis_db_get(ch->args[1]);
    key_exists = (existing_value != NULL);
  }

  if ((options.nx && key_exists) || (options.xx && !key_exists)) {
    add_null_reply(ch->client);
    return;
  }

  long long expiration = options.expiration;
  if (options.keepttl && existing_value != NULL) {
    expiration = existing_value->expiration;
  }

  redis_db_set(ch->args[1], ch->args[2], TYPE_STRING, expiration);

  if (options.get) {
    if (existing_value != NULL) {
      add_bulk_string_reply(ch->client, existing_value->data.str);
    } else {
      add_null_reply(ch->client);
    }
  } else {
    add_simple_string_reply(ch->client, "OK");
  }
}

void handle_get(CommandHandler *ch) {
  if (ch->arg_count != 2) {
    add_error_reply(ch->client, "ERR wrong number of arguments for 'get' command");
    return;
  }

  RedisValue *redis_value = redis_db_get(ch->args[1]);
  if (redis_value == NULL) {
    add_null_reply(ch->client);
  } else {
    add_bulk_string_reply(ch->client, redis_value->data.str);
  }
}