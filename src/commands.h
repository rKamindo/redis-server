#ifndef COMMAND_H
#define COMMAND_H

#include "command_handler.h"
#include "resp.h"
#include "ring_buffer.h"

typedef struct {
  long long expiration; // expiration time in milliseconds sinch epoch
  int nx;               // flag for NX option
  int xx;               // flag for XX option
  int keepttl;          // flag for KEEPTTL option
  int get;              // flag for get option
} SetOptions;

void handle_ping(CommandHandler *ch);
void handle_echo(CommandHandler *ch);
void handle_set(CommandHandler *ch);
void handle_get(CommandHandler *ch);
void handle_exist(CommandHandler *ch);
void handle_delete(CommandHandler *ch);
void handle_incr(CommandHandler *ch);
void handle_decr(CommandHandler *ch);
void handle_lpush(CommandHandler *ch);
void handle_rpush(CommandHandler *ch);
void handle_lrange(CommandHandler *ch);
void handle_config(CommandHandler *ch);
void handle_save(CommandHandler *ch);
void handle_dbsize(CommandHandler *ch);
void handle_info(CommandHandler *ch);
void handle_replconf(CommandHandler *ch);
void handle_psync(CommandHandler *ch);

void handle_simple_string_reply(CommandHandler *ch);

/* replication reply helpers */
void add_fullresync_reply(Client *client, char *master_replid, long long master_repl_offset);

void send_ping_command(Client *client);

// send replication handshake specific commands
void send_replconf_listening_port_command(Client *client, char *replica_port);
void send_replconf_capa_command(Client *client);
void send_psync_command(Client *client);

void propogate_command(CommandHandler *ch);

void add_error_reply(Client *client, const char *str);

#endif // COMMAND_H