#ifndef CLIENT_H
#define CLIENT_H

#include "database.h"
#include "handler.h"
#include "resp.h"        // Include this for the Parser definition
#include "ring_buffer.h" // Include this if ring_buffer is defined in a separate header

struct Parser;

typedef struct Client {
  int fd;
  ring_buffer input_buffer;
  ring_buffer output_buffer;
  struct Parser *parser;
  redis_db_t *db; // currently selected database
} Client;

Client *create_client(int fd, Handler *handler);
void select_client_db(Client *client, redis_db_t *db);
void flush_client_output(Client *client);
void destroy_client(Client *client);

#endif // CLIENT_H