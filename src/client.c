#include "client.h"
#include "database.h"
#include "redis-server.h"
#include "replication.h"
#include "server_config.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <unistd.h>

#define RING_BUFFER_SIZE 65536

Client *create_client(int fd) {
  Client *client = malloc(sizeof(Client));
  if (!client) return NULL;

  client->fd = fd;
  client->rdb_expected_bytes = 0;
  client->rdb_written_bytes = 0;
  client->rdb_received_bytes = 0;
  client->rdb_file_size = 0;
  client->rdb_file_offset = 0;
  client->tmp_rdb_fp = NULL;
  client->should_propogate_command = false;
  client->epoll_events = 0;

  // create a ring buffer of 64KB (adjust size as needed)
  if (rb_create(RING_BUFFER_SIZE, &client->input_buffer) != 0 ||
      rb_create(RING_BUFFER_SIZE, &client->output_buffer) != 0) {
    free(client);
    return NULL;
  }

  client->parser = malloc(sizeof(Parser));
  if (!client->parser) {
    rb_destroy(client->input_buffer);
    rb_destroy(client->output_buffer);
    free(client);
    return NULL;
  }

  int buffer_size = 1 << 20; // 1MB
  setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size));

  return client;
}

void select_client_db(Client *client, redis_db_t *db) { client->db = db; }

void destroy_client(Client *client) {
  close(client->fd);
  rb_destroy(client->input_buffer);
  rb_destroy(client->output_buffer);
  destroy_command_handler(client->parser->command_handler);
  destroy_parser(client->parser);
  free(client);
}

void flush_client_output(Client *client) {
  char *output_buf;
  size_t readable_len;

  // get the readable portion of the ring buffer
  while (1) {
    if (rb_readable(client->output_buffer, &output_buf, &readable_len) != 0) {
      fprintf(stderr, "Failed to get readable buffer\n");
      return;
    }

    if (readable_len == 0) {
      // no more data to send
      break;
    }

    ssize_t bytes_sent = write(client->fd, output_buf, readable_len);

    if (bytes_sent < 0) {
      if (errno == EINTR) {
        continue; // Interrupted system call, retry
      } else if (errno == EWOULDBLOCK) {
        // Socket would block; exit the loop
        break;
      } else {
        perror("Failed to send data to client");
        return; // Handle error appropriately
      }
    } else if (bytes_sent == 0) {
      // No bytes sent; this may indicate that the socket is not ready.
      break; // Exit if nothing was sent
    }

    rb_read(client->output_buffer, bytes_sent);
  }
}

void process_client_input(Client *client) {
  for (;;) {

    char *write_buf;
    size_t writable_len;

    // get the writable portion of the ring buffer
    if (rb_writable(client->input_buffer, &write_buf, &writable_len) != 0) {
      fprintf(stderr, "failed to get writable buffer\n");
      return;
    }

    if (writable_len == 0) {
      fprintf(stderr, "input buffer full\n");
      return;
    }

    // read from the socket into the writable portion of the ring buffer
    ssize_t bytes_received = read(client->fd, write_buf, writable_len);
    switch (bytes_received) {
    case -1:
      if (errno == EINTR) {
        continue;
      } else if (errno == EWOULDBLOCK) {
        flush_client_output(client);
        return;
      } else {
        perror("failed to read from client socket");
        return;
      }
    case 0:
      handle_client_disconnection(client);
      fprintf(stderr, "client disconnected\n");
      return;
    default:
      // update the write index of the ring buffer
      if (rb_write(client->input_buffer, bytes_received) != 0) {
        fprintf(stderr, "failed to update write index\n");
        return;
      }
    }

    char *read_buf;
    size_t readable_len;

    // get the readable portion of the ring buffer
    if (rb_readable(client->input_buffer, &read_buf, &readable_len) != 0) {
      fprintf(stderr, "failed to get readable buffer\n");
      return;
    }

    const char *begin = read_buf;
    const char *end = read_buf + readable_len;

    size_t bytes_parsed = parser_parse(client->parser, begin, end) - begin;

    if (client->should_propogate_command) {
      g_server_info.master_repl_offset += bytes_parsed;
      printf("propogating command\n");
      fflush(stdout);

      // // copy the bytes parsed into the repl_backlog buffer
      // char *repl_backlog_write_ptr;
      // size_t repl_backlog_writable_len;

      // if (rb_writable(g_repl_backlog, &repl_backlog_write_ptr, &repl_backlog_writable_len) != 0)
      // {
      //   perror("failed to get writable buffer");
      //   exit(EXIT_FAILURE);
      // }

      // if (repl_backlog_writable_len < bytes_parsed) {
      //   fprintf(stderr,
      //           "Warning: Replication backlog buffer full. Cannot propagate command (size %zu, "
      //           "available %zu).\n",
      //           bytes_parsed, repl_backlog_writable_len);
      // } else {
      //   memcpy(repl_backlog_write_ptr, client->input_buffer, bytes_parsed);
      //   if (rb_write(g_repl_backlog, bytes_parsed) != 0) {
      //     perror("failed to update write index for replication backlog");
      //     exit(EXIT_FAILURE);
      //   }

      // for each replica, copy to the replica's output buffer
      // and enable epoll to monitor for EPOLLOUT
      // for (int i = 0; i < MAX_REPLICAS; i++) {
      Client *replica_client = g_replica;
      if (replica_client != NULL) {
        char *replica_output_write_buf;
        size_t replica_output_writable_len;

        if (rb_writable(replica_client->output_buffer, &replica_output_write_buf,
                        &replica_output_writable_len) != 0) {
          fprintf(stderr, "failed to get writable buffer for replica %d's output buffer.\n",
                  replica_client->fd);
          // consider disconnecting this replica
        }

        // only copy it if we can fit it
        if (bytes_parsed <= replica_output_writable_len) {
          memcpy(replica_output_write_buf, begin, bytes_parsed);
          printf("copied command\n");
        }

        if (rb_write(replica_client->output_buffer, bytes_parsed) != 0) {
          fprintf(stderr, "Error: Failed to update write index for replica %d's output buffer.\n",
                  replica_client->fd);

          // continue;
        }

        // flush_client_output(replica_client);
        client_enable_write_events(replica_client);
      }
      // }
    }

    if (rb_read(client->input_buffer, bytes_parsed)) {
      fprintf(stderr, "failed to update read index\n");
      return;
    }

    if (bytes_received < writable_len) {
      flush_client_output(client);
      return;
    }
  }
}

void handle_client_disconnection(Client *client) {
  epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
  destroy_client(client);
}

void client_enable_read_events(Client *client) {
  if (!client) {
    perror("client_enable_read_events: client pointer is null\n");
    return;
  }
  struct epoll_event event;
  event.events = client->epoll_events | EPOLLIN;
  event.data.fd = client->fd;
  event.data.ptr = client;
  if (epoll_ctl(g_epoll_fd, EPOLL_CTL_MOD, client->fd, &event) == -1) {
    perror("epoll_ctl mod failed");
    exit(EXIT_FAILURE);
  }
  client->epoll_events = event.events;
}

void client_enable_write_events(Client *client) {
  if (!client) {
    perror("client_enable_write_events: client pointer is null\n");
    return;
  }
  struct epoll_event event;
  event.events = client->epoll_events | EPOLLOUT;
  event.data.fd = client->fd;
  event.data.ptr = client;
  if (epoll_ctl(g_epoll_fd, EPOLL_CTL_MOD, client->fd, &event) == -1) {
    perror("epoll_ctl mod failed");
    exit(EXIT_FAILURE);
  }
  client->epoll_events = event.events;
}

void client_disable_write_events(Client *client) {
  if (!client) {
    perror("client_disable_write_events: client pointer is null\n");
    return;
  }
  struct epoll_event event;
  event.events = client->epoll_events & ~EPOLLOUT;
  event.data.fd = client->fd;
  event.data.ptr = client;
  if (epoll_ctl(g_epoll_fd, EPOLL_CTL_MOD, client->fd, &event) == -1) {
    perror("epoll_ctl mod failed");
    exit(EXIT_FAILURE);
  }
  client->epoll_events = event.events;
}
