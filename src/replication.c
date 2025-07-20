#include "replication.h"
#include "commands.h"
#include "rdb.h"
#include <stdio.h>

void master_handle_replica_out(Client *client) {
  MasterReplicaState master_repl_state = client->master_repl_state;
  switch (master_repl_state) {
  case MASTER_REPL_STATE_SENDING_RDB_DATA:
    master_send_rdb_snapshot(client);
    break;
  }
}

void replica_handle_master_data(Client *master_client) {
  switch (master_client->repl_client_state) {
  case REPL_STATE_CONNECTING:
    send_ping_command(master_client);
    master_client->repl_client_state = REPL_STATE_SENT_PING;
    break;
  case REPL_STATE_RECEIVED_PONG:
    send_replconf_listening_port_command(master_client, g_server_config.port);
    master_client->repl_client_state = REPL_STATE_SENT_REPLCONF_PORT;
    break;
  case REPL_STATE_RECEIVED_REPLCONF_PORT_OK:
    send_replconf_capa_command(master_client);
    master_client->repl_client_state = REPL_STATE_SENT_REPLCONF_CAPA;
    break;
  case REPL_STATE_RECEIVED_REPLCONF_CAPA_OK:
    send_psync_command(master_client);
    master_client->repl_client_state = REPL_STATE_SENT_PSYNC;
    break;
  case REPL_STATE_RECEIVED_FULLRESYNC_RESPONSE:
    char *read_buf;
    size_t readable_len;

    if (rb_readable(master_client->input_buffer, &read_buf, &readable_len) != 0) {
      fprintf(stderr, "Failed to get readable buffer for client %d\n", master_client->fd);
      return; // error getting buffer state
    }

    if (readable_len < 4) {
      // not enough data for even a minimal header
      return;
    }

    // first character must be $
    if (read_buf[0] != '$') {
      fprintf(stderr, "protocol error for client %d, expected '$' for RDB header, got %c\n",
              master_client->fd, read_buf[0]);
    }

    char *crlf_pos = NULL;
    for (size_t i = 0; i < readable_len - 1; i++) {
      if (read_buf[i] == '\r' && read_buf[i + 1] == '\n') {
        crlf_pos = read_buf + i;
        break;
      }
    }

    if (crlf_pos == NULL) {
      // \r\n not found yet, need more data to complete the header
      return;
    }

    // calculate length between the $ and \r\n
    size_t size_str_len = crlf_pos - (read_buf + 1);
    if (size_str_len == 0) {
      fprintf(stderr,
              "error in parsing bulk string header for rdb transfer, empty rdb size string\n");
    }

    int64_t length = 0;

    char *end_ptr;
    length = strtol(read_buf + 1, &end_ptr, 10);

    size_t bytes_to_consume =
        (crlf_pos - read_buf) + 2; // length from read_buf to \r, plus 2 for \r\n
    if (rb_read(master_client->input_buffer, bytes_to_consume) != 0) {
      fprintf(stderr, "failed to consume RDB header from buffer for client %d\n",
              master_client->fd);
      return;
    }

    master_client->rdb_expected_bytes = length;
    master_client->rdb_received_bytes = 0;
    master_client->repl_client_state = REPL_STATE_RECEIVING_RDB_DATA;

    // proceed to receive RDB data, as some might already be in the buffer
    // after the header was confused
    replica_receive_rdb_snapshot(master_client);
    break;
  case REPL_STATE_RECEIVING_RDB_DATA:
    replica_receive_rdb_snapshot(master_client);
    break;
  case REPL_STATE_READY:
    break;
  default:
  }
}
