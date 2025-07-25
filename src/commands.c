#include "commands.h"
#include "client.h"
#include "command_handler.h"
#include "database.h"
#include "linked_list.h"
#include "redis-server.h"
#include "server_config.h"
#include "sys/time.h"
#include "util.h"
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>

#define MAX_PATH_LENGTH 256
#define INFO_BUFFER_SIZE 2048 // a buffer for various INFO fields

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

void add_array_reply(Client *client, char **array, int length) {
  write_begin_array(client->output_buffer, length);
  if (array != NULL) {
    for (int i = 0; i < length; i++) {
      int64_t len = strlen(array[i]);
      write_begin_bulk_string(client->output_buffer, len);
      write_chars(client->output_buffer, array[i]);
      write_end_bulk_string(client->output_buffer);
    }
  }
  write_end_array(client->output_buffer);
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

void add_psync_reply(Client *client, char *master_replid, long long master_repl_offset) {
  printf("add_psync_reply called with master_replid: %s, master_repl_offset: %lld\n", master_replid,
         master_repl_offset);
  fflush(stdout);
  char full_resync_reply[80];
  snprintf(full_resync_reply, 80, "FULLRESYNC %s %lld", master_replid, master_repl_offset);
  add_simple_string_reply(client, full_resync_reply);
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
  printf("handling ping\n");
  fflush(stdout);
  Client *client = ch->client;
  if (ch->arg_count == 1) {
    add_simple_string_reply(client, "PONG");
  } else if (ch->arg_count == 2) {
    add_simple_string_reply(client, ch->args[1]);
  } else {
    add_error_reply(client, "ERR wrong number of arguments for 'ping' command");
  }
}

void handle_echo(CommandHandler *ch) {
  Client *client = ch->client;
  if (ch->arg_count != 2) {
    add_error_reply(client, "ERR wrong number of arguments for 'echo' command");
    return;
  }
  add_simple_string_reply(client, ch->args[1]);
}

void handle_set(CommandHandler *ch) {
  Client *client = ch->client;
  if (ch->arg_count < 3) {
    add_error_reply(client, "ERR wrong number of arguments for 'set' command");
    return;
  }

  SetOptions options = {0};
  int parse_result = parse_set_options(ch->args + 3, ch->arg_count - 3, &options);

  switch (parse_result) {
  case ERR_SYNTAX:
    add_error_reply(client, "ERR syntax error");
    return;
  case ERR_VALUE:
    add_error_reply(client, "ERR value is not an integer or out of range");
    return;
  }

  if (options.expiration < 0) {
    add_error_reply(client, "ERR expiration must be a non-negative integer");
    return;
  }

  RedisValue *existing_value = NULL;
  bool key_exists = false;
  char *old_value = NULL;

  // only get the existing value if we need to check conditions (NX, XX) or respond to GET
  // option
  if (options.nx || options.xx || options.get || options.keepttl) {
    existing_value = redis_db_get(client->db, ch->args[1]);
    if (existing_value != NULL) {
      key_exists = true;
    }
  }

  if ((options.nx && key_exists) || (options.xx && !key_exists)) {
    add_null_reply(client);
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

  redis_db_set(client->db, ch->args[1], ch->args[2], TYPE_STRING, expiration);

  if (options.get) {
    if (old_value) {
      add_bulk_string_reply(client, old_value);
      free(old_value);
    } else {
      add_null_reply(client);
    }
  } else {
    add_simple_string_reply(client, "OK");
  }
}

void handle_get(CommandHandler *ch) {
  Client *client = ch->client;
  if (ch->arg_count != 2) {
    add_error_reply(client, "ERR wrong number of arguments for 'get' command");
    return;
  }

  RedisValue *redis_value = redis_db_get(client->db, ch->args[1]);
  if (redis_value == NULL) {
    add_null_reply(client);
  } else if (redis_value->type != TYPE_STRING) {
    add_error_reply(client, "ERR Operation against a key holding the wrong kind of value");
  } else {
    add_bulk_string_reply(client, redis_value->data.str);
  }
}

void handle_exist(CommandHandler *ch) {
  Client *client = ch->client;
  if (ch->arg_count < 2) {
    add_error_reply(client, "ERR wrong number of arguments for 'exist' command");
    return;
  }

  int count = 0;
  for (int i = 1; i < ch->arg_count; i++) {
    if (redis_db_exist(client->db, ch->args[i])) {
      count += 1;
    }
  }

  add_integer_reply(client, count);
}

void handle_delete(CommandHandler *ch) {
  Client *client = ch->client;
  if (ch->arg_count < 2) {
    add_error_reply(client, "ERR wrong number of arguments for 'delete' command");
    return;
  }

  for (int i = 1; i < ch->arg_count; i++) {
    redis_db_delete(client->db, ch->args[i]);
  }
  add_simple_string_reply(client, "OK");
}

void handle_incr_decr(CommandHandler *ch, int increment) {
  Client *client = ch->client;
  if (ch->arg_count < 2) {
    add_error_reply(client, "ERR wrong number of arguments for 'incr/decr' command");
    return;
  }

  RedisValue *redis_value = redis_db_get(client->db, ch->args[1]);
  long current_value = 0;

  // check if the key exists and parse its value
  if (redis_value != NULL) {
    int parse_result = parse_integer(redis_value->data.str, &current_value);
    if (parse_result == ERR_VALUE) {
      add_error_reply(client, "ERR value is not an integer");
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
  redis_db_set(client->db, ch->args[1], new_value_str, TYPE_STRING, 0);
  add_integer_reply(client, new_value);
}

void handle_incr(CommandHandler *ch) { handle_incr_decr(ch, 1); }

void handle_decr(CommandHandler *ch) { handle_incr_decr(ch, -1); }

void handle_lpush(CommandHandler *ch) {
  Client *client = ch->client;
  if (ch->arg_count < 3) { // at least 2 arguments: key and one element
    add_error_reply(client, "ERR wrong number of arguments for 'lpush' command");
    return;
  }
  const char *key = ch->args[1];
  int length = 0;
  for (int i = 2; i < ch->arg_count; i++) {
    int result = redis_db_lpush(client->db, key, ch->args[i], &length); // capture the result
    if (result == ERR_TYPE_MISMATCH) { // check for type mismatch error
      add_error_reply(client, "ERR Operation against a key holding the wrong kind of value");
      return;
    }
  }
  add_integer_reply(client, length);
}

void handle_rpush(CommandHandler *ch) {
  Client *client = ch->client;
  if (ch->arg_count < 3) { // at least 2 arguments: key and one element
    add_error_reply(client, "ERR wrong number of arguments for 'rpush' command");
    return;
  }
  const char *key = ch->args[1];
  int length = 0; // stores the length of the list after operation
  for (int i = 2; i < ch->arg_count; i++) {
    int result = redis_db_rpush(client->db, key, ch->args[i], &length); //
    if (result == ERR_TYPE_MISMATCH) { // check for type mismatch error
      add_error_reply(client, "ERR Operation against a key holding the wrong kind of value");
      return;
    }
  }
  add_integer_reply(client, length);
}

void handle_lrange(CommandHandler *ch) {
  Client *client = ch->client;
  if (ch->arg_count < 4) { // needs 3 arguments: LRANGE key start end
    add_error_reply(client, "ERR wrong number of arguments for 'rpush' command");
    return;
  }

  char **range;
  int range_length;
  char *list_key = ch->args[1];
  long start = 0;
  long end = 0;
  parse_integer(ch->args[2], &start);
  parse_integer(ch->args[3], &end);

  int error = redis_db_lrange(client->db, list_key, start, end, &range, &range_length);
  if (error == ERR_TYPE_MISMATCH) { // check for type mismatch error
    add_error_reply(client, "ERR Operation against a key holding the wrong kind of value");
    return;
  }

  add_array_reply(client, range, range_length);
  cleanup_lrange_result(range, range_length);
}

void handle_config(CommandHandler *ch) {
  Client *client = ch->client;
  if (ch->arg_count < 3) {
    add_error_reply(client, "ERR wrong number of arguments for 'config get' command");
    return;
  }

  int param_count = ch->arg_count - 2;
  char *response[param_count * 2];
  int response_index = 0;

  if (strcmp(ch->args[1], "GET") == 0) {
    for (int i = 0; i < param_count; i++) {
      const char *param = ch->args[i + 2];
      if (strcmp(param, "dir") == 0) {
        response[response_index++] = "dir";
        response[response_index++] = g_server_config.dir;
      } else if (strcmp(param, "dbfilename") == 0) {
        response[response_index++] = "dbfilename";
        response[response_index++] = g_server_config.dbfilename;
      } else {
        add_error_reply(client, "ERR Unknown config parameter");
        return;
      }
    }
    add_array_reply(client, response, response_index);
  }
}

void handle_save(CommandHandler *ch) {
  Client *client = ch->client;
  if (ch->arg_count != 1) {
    add_error_reply(client, "ERR wrong number of arguments for 'save' command");
  }

  redis_db_save(client->db);

  add_simple_string_reply(client, "OK");
}

// gets the key count for the currently selected db
void handle_dbsize(CommandHandler *ch) {
  Client *client = ch->client;
  size_t dbsize = redis_db_dbsize(client->db);

  int size = (int)dbsize;

  add_integer_reply(client, size);
}

void handle_info(CommandHandler *ch) {
  Client *client = ch->client;
  // only support the "role" key for now
  char info_output_buffer[INFO_BUFFER_SIZE];
  int current_offset = 0;
  current_offset += snprintf(info_output_buffer + current_offset,
                             sizeof(info_output_buffer) - current_offset, "# Replication\r\n");

  current_offset +=
      snprintf(info_output_buffer + current_offset, sizeof(info_output_buffer) - current_offset,
               "role:%s\r\n", g_server_info.role);

  current_offset +=
      snprintf(info_output_buffer + current_offset, sizeof(info_output_buffer) - current_offset,
               "master_replid:%s\r\n", g_server_info.master_replid);

  current_offset +=
      snprintf(info_output_buffer + current_offset, sizeof(info_output_buffer) - current_offset,
               "master_repl_offset:%lld\r\n", g_server_info.master_repl_offset);
  add_bulk_string_reply(client, info_output_buffer);
}

void handle_replconf(CommandHandler *ch) { add_simple_string_reply(ch->client, "OK"); }

void handle_simple_string_reply(CommandHandler *ch) {
  Client *client = ch->client;
  char *reply = ch->buf;
  if (ch->client->type == CLIENT_TYPE_MASTER) {
    // replica is handling responses from master
    ReplicaClientState repl_client_state = client->repl_client_state;
    if (repl_client_state == REPL_STATE_SENT_PING) {
      if (strcmp(reply, "PONG") == 0) {
        client->repl_client_state = REPL_STATE_RECEIVED_PONG;
      } else {
        fprintf(stderr, "expected PONG in REPL_STATE_SENT_PING, got '%s'\n", reply);
        client->repl_client_state = REPL_STATE_ERROR;
      }
    } else if (repl_client_state == REPL_STATE_SENT_REPLCONF_PORT) {
      if (strcmp(reply, "OK") == 0) {
        client->repl_client_state = REPL_STATE_RECEIVED_REPLCONF_PORT_OK;
      } else {
        fprintf(stderr, "expected OK in REPL_STATE_SENT_REPLCONF_PORT, got '%s'\n", reply);
        client->repl_client_state = REPL_STATE_ERROR;
      }
    } else if (repl_client_state == REPL_STATE_SENT_REPLCONF_CAPA) {
      if (strcmp(reply, "OK") == 0) {
        client->repl_client_state = REPL_STATE_RECEIVED_REPLCONF_CAPA_OK;
      } else {
        fprintf(stderr, "error: expected OK in REPL_STATE_SENT_REPLCONF_CAPA, got '%s'\n", reply);
        client->repl_client_state = REPL_STATE_ERROR;
      };
    } else if (repl_client_state == REPL_STATE_SENT_PSYNC) {
      if (strncmp(reply, "FULLRESYNC", 9) == 0) {
        sscanf("FULLRESYNC %40s %lld", g_server_info.master_replid,
               g_server_info.master_repl_offset);

        printf("Replica sync: Master ID: %s, Offset: %lld\n", g_server_info.master_replid,
               g_server_info.master_repl_offset);
        client->repl_client_state = REPL_STATE_RECEIVED_FULLRESYNC_RESPONSE;
      } else {
        fprintf(stderr, "error: expected reply in REPL_STATE_SENT_PSYNC, got '%s'\n", reply);
        client->repl_client_state = REPL_STATE_ERROR;
      }
    }
  }
}

void handle_psync(CommandHandler *ch) {
  Client *client = ch->client;
  add_psync_reply(client, g_server_info.master_replid, g_server_info.master_repl_offset);
  client->type = CLIENT_TYPE_REPLICA;
  // send $<length_of_file>\r\n<file_content>
  // call save
  redis_db_save(client->db);

  char *db_file_path = construct_file_path(g_server_config.dir, g_server_config.dbfilename);
  printf("%s\n", db_file_path);
  FILE *file = fopen(db_file_path, "rb");
  if (!file) {
    perror("Failed to open RDB file or it does not exist\n");
  }
  int rdb_file_fd = fileno(file);
  struct stat statbuf;
  fstat(rdb_file_fd, &statbuf);
  off_t db_file_size = (off_t)statbuf.st_size;

  client->rdb_fd = rdb_file_fd;
  client->rdb_file_size = db_file_size;
  printf("rdb file size: %ld\n", db_file_size);
  client->rdb_file_offset = 0;

  client->master_repl_state = MASTER_REPL_STATE_SENDING_RDB_DATA;
  write_begin_bulk_string(client->output_buffer, (int64_t)db_file_size);
  client_enable_write_events(client);
}

void send_ping_command(Client *client) {
  char *ping_cmd[] = {"PING"};
  add_array_reply(client, ping_cmd, 1);
  flush_client_output(client);
  client->repl_client_state = REPL_STATE_SENT_PING;
  printf("sent ping command\n");
}

void send_replconf_listening_port_command(Client *client, char *replica_port) {
  char *args[3] = {"REPLCONF", "listening-port", replica_port};
  add_array_reply(client, args, 3);
  flush_client_output(client);
  client->repl_client_state = REPL_STATE_SENT_REPLCONF_PORT;
  printf("sent repl conf listening port\n");
}

void send_replconf_capa_command(Client *client) {
  // safely hardcode capabilites for now
  char *args[3] = {"REPLCONF", "capa", "psync2"};
  add_array_reply(client, args, 3);
  flush_client_output(client);
  client->repl_client_state = REPL_STATE_SENT_REPLCONF_CAPA;
  printf("sent replconf capa command with args: capa psync2\n");
}

void send_psync_command(Client *client) {
  // initially hardcode PSYNC ? -1, for testing handshake
  char *args[3] = {"PSYNC", "?", "-1"};
  add_array_reply(client, args, 3);
  flush_client_output(client);
  client->repl_client_state = REPL_STATE_SENT_PSYNC;
  printf("sent psync command with args: ? -1\n");
}