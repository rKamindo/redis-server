#include "commands.h"
#include "command_handler.h"
#include "database.h"
#include "linked_list.h"
#include "sys/time.h"
#include "util.h"
#include <errno.h>
#include <stddef.h>
#include <stdio.h>

void add_simple_string_reply(Client *client, const char *str) {
  write_begin_simple_string(client->output_buffer);
  write_chars(client->output_buffer, str);
  write_end_simple_string(client->output_buffer);
}

void add_bulk_string_reply(Client *client, const char *str) {
  int64_t len = strlen(str);
  write_begin_bulk_string(client->output_buffer, len);
  write_chars(client->output_buffer, str);
  write_end_bulk_string(client->output_buffer);
}

void add_null_reply(Client *client) {
  write_begin_bulk_string(client->output_buffer, -1); // indicate that this is a null bulk string
  write_end_bulk_string(client->output_buffer);
}

void add_error_reply(Client *client, const char *str) {
  write_begin_error(client->output_buffer);
  write_chars(client->output_buffer, str);
  write_end_error(client->output_buffer);
}

void add_integer_reply(Client *client, int integer) {
  char number_str[32];
  snprintf(number_str, sizeof(number_str), "%d", integer);

  write_begin_integer(client->output_buffer);
  write_chars(client->output_buffer, number_str);
  write_end_integer(client->output_buffer);
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
      if (options->expiration) return ERR_SYNTAX;
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
  case ERR_VALUE:
    add_error_reply(ch->client, "ERR value is not an integer or out of range");
    return;
  }

  RedisValue *existing_value = NULL;
  bool key_exists = false;
  char *old_value = NULL;

  // only get the existing value if we need to check conditions (NX, XX) or respond to GET
  // option
  if (options.nx || options.xx || options.get || options.keepttl) {
    existing_value = redis_db_get(ch->args[1]);
    if (existing_value != NULL) {
      key_exists = true;
    }
  }

  if ((options.nx && key_exists) || (options.xx && !key_exists)) {
    add_null_reply(ch->client);
    return;
  }

  long long expiration = options.expiration;
  if (options.keepttl && existing_value != NULL) {
    expiration = existing_value->expiration;
  }

  if (options.get) {
    if (existing_value) {
      old_value = strdup(existing_value->data.str); // store the old value
    }
  }

  redis_db_set(ch->args[1], ch->args[2], TYPE_STRING, expiration);

  if (options.get) {
    if (old_value) {
      add_bulk_string_reply(ch->client, old_value);
      free(old_value);
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
  } else if (redis_value->type != TYPE_STRING) {
    add_error_reply(ch->client, "ERR Operation against a key holding the wrong kind of value");
  } else {
    add_bulk_string_reply(ch->client, redis_value->data.str);
  }
}

void handle_exist(CommandHandler *ch) {
  if (ch->arg_count < 2) {
    add_error_reply(ch->client, "ERR wrong number of arguments for 'exist' command");
    return;
  }

  int count = 0;
  for (int i = 1; i < ch->arg_count; i++) {
    if (redis_db_exist(ch->args[i])) {
      count += 1;
    }
  }

  add_integer_reply(ch->client, count);
}

void handle_delete(CommandHandler *ch) {
  if (ch->arg_count < 2) {
    add_error_reply(ch->client, "ERR wrong number of arguments for 'delete' command");
    return;
  }

  for (int i = 1; i < ch->arg_count; i++) {
    redis_db_delete(ch->args[i]);
  }
  add_simple_string_reply(ch->client, "OK");
}

void handle_incr_decr(CommandHandler *ch, int increment) {
  if (ch->arg_count < 2) {
    add_error_reply(ch->client, "ERR wrong number of arguments for 'incr/decr' command");
    return;
  }

  RedisValue *redis_value = redis_db_get(ch->args[1]);
  long current_value = 0;

  // check if the key exists and parse its value
  if (redis_value != NULL) {
    int parse_result = parse_integer(redis_value->data.str, &current_value);
    if (parse_result == ERR_VALUE) {
      add_error_reply(ch->client, "ERR value is not an integer");
      return;
    }
  } else {
    // if the key does not exist, initialize current_value to 0
    current_value = 0;
  }

  // calculate the new value based on increment or decrement
  long new_value = current_value + increment;
  char new_value_str[21];
  snprintf(new_value_str, sizeof(new_value_str), "%ld", new_value);

  // set the new value in the database
  redis_db_set(ch->args[1], new_value_str, TYPE_STRING, 0);
  add_integer_reply(ch->client, new_value);
}

void handle_incr(CommandHandler *ch) { handle_incr_decr(ch, 1); }

void handle_decr(CommandHandler *ch) { handle_incr_decr(ch, -1); }

void handle_lpush(CommandHandler *ch) {
  if (ch->arg_count < 3) { // at least 2 arguments: key and one element
    add_error_reply(ch->client, "ERR wrong number of arguments for 'lpush' command");
    return;
  }
  const char *key = ch->args[1];
  int length = 0;
  for (int i = 2; i < ch->arg_count; i++) {
    int result = redis_db_lpush(key, ch->args[i], &length); // capture the result
    if (result == ERR_TYPE_MISMATCH) {                      // check for type mismatch error
      add_error_reply(ch->client, "ERR Operation against a key holding the wrong kind of value");
      return;
    }
  }
  add_integer_reply(ch->client, length);
}

void handle_rpush(CommandHandler *ch) {
  if (ch->arg_count < 3) { // at least 2 arguments: key and one element
    add_error_reply(ch->client, "ERR wrong number of arguments for 'rpush' command");
    return;
  }
  const char *key = ch->args[1];
  int length = 0; // stores the length of the list after operation
  for (int i = 2; i < ch->arg_count; i++) {
    int result = redis_db_rpush(key, ch->args[i], &length); //
    if (result == ERR_TYPE_MISMATCH) {                      // check for type mismatch error
      add_error_reply(ch->client, "ERR Operation against a key holding the wrong kind of value");
      return;
    }
  }
  add_integer_reply(ch->client, length);
}
