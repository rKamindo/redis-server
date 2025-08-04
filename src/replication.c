#include "replication.h"
#include "commands.h"
#include "rdb.h"
#include "redis-server.h"
#include "server_config.h"
#include "util.h"
#include <errno.h>
#include <stdio.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

void master_handle_replica_out(Client *client) {
  MasterReplicaState master_repl_state = client->master_repl_state;
  switch (master_repl_state) {
  case MASTER_REPL_STATE_PROPAGATE:
    flush_client_output(client);
    break;
  case MASTER_REPL_STATE_SENDING_RDB_DATA:
    master_send_rdb_snapshot(client);
    break;
  case MASTER_REPL_STATE_PSYNC:
    continue_psync(client, g_server_info.repl_backlog); // create this function, once it is
                                                        // complete, set state to propogate
    break;
  }
}

void replica_handle_master_data(Client *master_client) {
  switch (master_client->repl_client_state) {
  case REPL_STATE_CONNECTING:
    send_ping_command(master_client);
    master_client->repl_client_state = REPL_STATE_SENT_PING;
    break;
  case REPL_STATE_SENT_PING:
  case REPL_STATE_SENT_REPLCONF_PORT:
  case REPL_STATE_SENT_REPLCONF_CAPA:
  case REPL_STATE_SENT_PSYNC:
    // we are expecting replys from master
    process_client_input(master_client);
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
    process_client_input(master_client);
    break;
  default:
    fprintf(stderr, "invalid state for replica handling master data\n");
  }
}

/*
Add a replica to the array of replicas. Write commands will be propogated to replicas.
*/
void add_replica(Client *client) {
  if (!client) {
    fprintf(stderr, "add_replica: client is null");
  }
  client->type = CLIENT_TYPE_REPLICA;
  client->should_reply = false;
  g_server_info.replicas[g_server_info.num_replicas++] = client;
}

/*
Remove a replica from the array of replicas.
Swaps the replica to be re
*/
void remove_replica(Client *client) {
  if (!client) {
    fprintf(stderr, "remove_replica: client is null");
  }

  int num_replicas = g_server_info.num_replicas;
  Client *replica = NULL;
  int replica_pos = -1;
  for (int i = 0; i < num_replicas; i++) {
    if (g_server_info.replicas[i] == client) {
      replica_pos = i;
      break;
    }
  }

  if (replica_pos == -1) {
    fprintf(stderr, "remove_replica: replica not found in replicas");
  }

  g_server_info.replicas[replica_pos] = g_server_info.replicas[num_replicas - 1];
  g_server_info.num_replicas--;
}

/*
Sends the rdb snapshot to a replica, using the sendfile() syscall, which avoids copying into
userspace.
*/
void master_send_rdb_snapshot(Client *client) {
  if (!client->type == CLIENT_TYPE_REPLICA) {
    perror("cannot send rdb file to a non-replica client\n");
    return;
  }

  size_t remaining_bytes = client->rdb_file_size - client->rdb_file_offset;
  printf("remaining bytes is %ld\n", remaining_bytes);

  while (1) {
    ssize_t bytes_sent =
        sendfile(client->fd, client->rdb_fd, &client->rdb_file_offset, remaining_bytes);
    //  printf("bytes sent: %ld\n", bytes_sent);
    if (bytes_sent == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      } else if (errno == EINTR) {
        continue;
      } else {
        perror("sendfile failed\n");
        return;
      }
    } else if (bytes_sent == 0) {
      break;
    }
  }

  if (client->rdb_file_offset == client->rdb_file_size) {
    printf("RDB file transmission complete for client %d\n", client->fd);
    client_disable_write_events(client);
    client->master_repl_state = MASTER_REPL_STATE_PROPAGATE;
  }
}

void replica_receive_rdb_snapshot(Client *client) {
  ssize_t bytes_read;

  // open a temporary file for writing
  if (client->tmp_rdb_fp == NULL) {
    char *file_path = construct_file_path(g_server_config.dir, "temp_snapshot.rdb");
    client->tmp_rdb_fp = fopen(file_path, "wb");
    if (!client->tmp_rdb_fp) {
      perror("could not open temporary RDB file for writing received snapshot\n");
      return;
    }
    printf("opened temporary file: %s\n", file_path);
    free(file_path);
  }

  // continusly read from socket into ring buffer
  // and drain the ring buffer into the file.
  while (1) {
    char *write_buf;
    char *read_buf;
    size_t writable_len;
    size_t readable_len;
    if (client->rdb_received_bytes < client->rdb_expected_bytes) {
      // we haven't received the expected number of bytes yet
      // get the writable portion of the ring buffer
      if (rb_writable(client->input_buffer, &write_buf, &writable_len) != 0) {
        fprintf(stderr, "failed to get writable buffer\n");
        return;
      }

      if (writable_len == 0) {
        fprintf(stderr, "input buffer full\n");
        return;
      }

      ssize_t remaining_bytes_to_read = client->rdb_expected_bytes - client->rdb_received_bytes;

      bytes_read = read(client->fd, write_buf, remaining_bytes_to_read);

      if (bytes_read == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          return;
        } else if (errno == EINTR) {
          continue;
        } else {
          perror("sendfile failed\n");
          return;
        }
      } else if (bytes_read == 0) {
        break;
      }

      if (rb_write(client->input_buffer, bytes_read)) {
        fprintf(stderr, "failed to update write index\n");
      }

      client->rdb_received_bytes += bytes_read;

      printf("receiving rdb data from master: read %ld bytes into input buffer\n", bytes_read);

      if (bytes_read == 0) {
        // master closed connection. signals end of RDB transfer
        printf("master closed connection\n");
        break;
      } else if (bytes_read == -1) {
        if (errno == EINTR)
          continue; // interrupted retry
        else {
          perror("error reading from master socket.");
          return;
        }
      }
    }

    if (rb_readable(client->input_buffer, &read_buf, &readable_len) != 0) {
      fprintf(stderr, "failed to get readable buffer\n");
      return;
    }

    if (readable_len == 0) {
      fprintf(stderr, "input buffer empty, nothing to write to disk\n");
    }

    // write to disk
    ssize_t bytes_written = fwrite(read_buf, 1, bytes_read, client->tmp_rdb_fp);

    printf("writing rdb data from buffer: wrote %ld bytes into temp file\n", bytes_read);

    client->rdb_written_bytes += bytes_written;
    if (rb_read(client->input_buffer, bytes_written) != 0) {
      fprintf(stderr, "error updating read index for ring buffer\n");
    }

    if (client->rdb_written_bytes == client->rdb_expected_bytes) {
      printf("RDB snapshot completely received and written to temp file: %lld bytes.\n",
             client->rdb_expected_bytes);

      fclose(client->tmp_rdb_fp);

      rdb_load_data_from_file(client->db, g_server_config.dir, "temp_snapshot.rdb");

      client->repl_client_state = REPL_STATE_READY;
      client_enable_read_events(client); // start monitoring for EPOLLIN from master
      return;
    }
  }
}

void begin_fullresync(Client *client) {
  printf("beginning full resync\n");
  add_fullresync_reply(client, g_server_info.master_replid, g_server_info.master_repl_offset);
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
  // send $<length_of_file>\r\n<file_content>

  write_begin_bulk_string(client->output_buffer, (int64_t)db_file_size);
  client_enable_write_events(client);
}

void continue_psync(Client *client, ring_buffer repl_backlog) {
  printf("continuing psync\n");
  long long master_offset = g_server_info.master_repl_offset;
  long long replica_offset = client->repl_offset;

  printf("master's offset is %lld\n", master_offset);
  printf("replica's offset is %lld\n", replica_offset);

  // check if the replica has caught up
  if (replica_offset == master_offset) {
    printf("replica has caught up, transitioning to PROPAGATE state.\n");
    client->master_repl_state = MASTER_REPL_STATE_PROPAGATE;
    client_disable_write_events(client); // no more backlog to send
    return;
  } else if (replica_offset >= master_offset) {
    // invalid state, does not make sense, replica can't be ahead of master
    fprintf(stderr, "replica offset is ahead of master offset\n. disconnecting client.\n");
    handle_client_disconnection(client);
  }

  // determine how much data to send in this iteration.
  // the amount to send is the difference between the master's offset and the replica's
  long long bytes_to_send = master_offset - replica_offset;
  if (bytes_to_send <= 0) {
    // this case should not be reached if the catchup check is working correctly
    return;
  }

  char *repl_backlog_read_buf;
  size_t repl_backlog_readable_len;

  if (rb_readable(repl_backlog, &repl_backlog_read_buf, &repl_backlog_readable_len) != 0) {
    fprintf(stderr, "failed to get readable buf for repl backlog\n");
  }

  if (bytes_to_send > repl_backlog_readable_len) {
    // this should never happen
    fprintf(stderr, "replica offset is outside backlog, disconnecting client\n");
    handle_client_disconnection(client);
    return;
  }

  size_t relative_offset = replica_offset - g_server_info.repl_backlog_base_offset;
  char *source_ptr = repl_backlog_read_buf + relative_offset;

  memcpy(client->output_buffer, source_ptr, bytes_to_send);

  size_t bytes_sent = flush_client_output(client);
  if (bytes_sent > 0) {
    client->repl_offset += bytes_sent;
    printf("sent %ld bytes to replica during PSYNC\n", bytes_sent);
  }
}