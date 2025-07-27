#ifndef CLIENT_H
#define CLIENT_H

#include "database.h"
#include "handler.h"
#include "resp.h"        // Include this for the Parser definition
#include "ring_buffer.h" // Include this if ring_buffer is defined in a separate header
#include <stdio.h>

struct Parser;

typedef enum { CLIENT_TYPE_REGULAR, CLIENT_TYPE_REPLICA, CLIENT_TYPE_MASTER } ClientType;

typedef enum {
  REPL_STATE_CONNECTING,
  REPL_STATE_SENT_PING,
  REPL_STATE_RECEIVED_PONG,
  REPL_STATE_SENT_REPLCONF_PORT,
  REPL_STATE_RECEIVED_REPLCONF_PORT_OK,
  REPL_STATE_SENT_REPLCONF_CAPA,
  REPL_STATE_RECEIVED_REPLCONF_CAPA_OK,
  REPL_STATE_SENT_PSYNC,
  REPL_STATE_RECEIVED_FULLRESYNC_RESPONSE,
  REPL_STATE_RECEIVING_RDB_DATA,
  REPL_STATE_READY,
  REPL_STATE_ERROR
} ReplicaClientState;

typedef enum { MASTER_REPL_STATE_SENDING_RDB_DATA, MASTER_REPL_STATE_PROPOGATE } MasterReplicaState;

typedef struct Client {
  int fd;
  ring_buffer input_buffer;
  ring_buffer output_buffer;
  struct Parser *parser;
  redis_db_t *db; // currently selected database
  ClientType type;
  int epoll_events; // epoll events that socket is being monitored for

  /* replication specific fields */
  // master specific fields
  MasterReplicaState master_repl_state;
  int rdb_fd; // pointer of rdb file being sent,
  off_t rdb_file_offset;
  off_t rdb_file_size;

  // replica specific fields
  ReplicaClientState repl_client_state;
  long long rdb_expected_bytes;
  long long rdb_received_bytes;
  long long rdb_written_bytes;
  FILE *tmp_rdb_fp; // temporary file for writing rdb from master

  // used to determine whether to propogate commands
  bool should_propogate_command;
  // used to determine whether this client should be replied to
  bool should_reply;
} Client;

Client *create_client(int fd);
void select_client_db(Client *client, redis_db_t *db);
void flush_client_output(Client *client);
void destroy_client(Client *client);
void process_client_input(Client *client);
void handle_client_disconnection(Client *client);
void client_enable_read_events(Client *client);

#endif // CLIENT_H