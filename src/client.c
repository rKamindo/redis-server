#include "client.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#define RING_BUFFER_SIZE 65536

Client *create_client(int fd, Handler *handler) {
  Client *client = malloc(sizeof(Client));
  if (!client) return NULL;

  client->fd = fd;

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