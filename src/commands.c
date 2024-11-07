#include "commands.h"
#include "command_handler.h"
#include "database.h"
#include "sys/time.h"
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

void parse_set_options(char **args, int args_count, SetOptions *options) {
  memset(options, 0, sizeof(SetOptions)); // initialize options

  for (int i = 0; i < args_count; i++) {
    if (strcmp(args[i], "EX") == 0 && i + 1 < args_count) {
      int seconds = atoi(args[++i]);
      options->expiration = current_time_millis() + seconds * 1000;
    } else if (strcmp(args[i], "PX") == 0 && i + 1 < args_count) {
      int milliseconds = atoi(args[++i]);
      options->expiration = current_time_millis() + milliseconds;
    } else if (strcmp(args[i], "EXAT") == 0 && i + 1 < args_count) {
      options->expiration = atoi(args[++i]) * 1000;
    } else if (strcmp(args[i], "PXAT") == 0 && i + 1 < args_count) {
      options->expiration = atoi(args[++i]);
    } else if (strcmp(args[i], "NX") == 0) {
      options->nx = 1;
    } else if (strcmp(args[i], "XX") == 0) {
      options->xx = 1;
    } else if (strcmp(args[i], "KEEPTTL") == 0) {
      options->keepttl = 1;
    } else if (strcmp(args[i], "GET") == 0) {
      options->get = 1;
    }
  }
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
  parse_set_options(ch->args + 3, ch->arg_count - 3, &options);

  RedisValue *existing_value = redis_db_get(ch->args[1]);
  int key_exists = (existing_value != NULL);

  if ((options.nx && key_exists) || (options.xx && !key_exists)) {
    add_null_reply(ch->client);
    return;
  }

  existing_value = NULL;
  if (options.get) {
    existing_value = redis_db_get(ch->args[1]);
    if (existing_value != NULL) {
      add_bulk_string_reply(ch->client, existing_value->data.str);
    } else {
      add_null_reply(ch->client);
    }
  }

  long long expiration = options.expiration;
  if (options.keepttl && existing_value != NULL) {
    expiration = existing_value->expiration;
  }

  redis_db_set(ch->args[1], ch->args[2], TYPE_STRING, expiration);

  if (!options.get) {
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