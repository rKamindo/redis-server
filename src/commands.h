#ifndef COMMAND_H
#define COMMAND_H

#include "command_handler.h"
#include "resp.h"

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

void add_error_reply(Client *client, const char *str);

#endif // COMMAND_H