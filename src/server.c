#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "client.h"
#include "command_handler.h"
#include "handler.h"
#include "khash.h"
#include "resp.h"
#include "ring_buffer.h"
#include "server.h"
#include <errno.h>

#define DEFAULT_PORT 6379
#define RING_BUFFER_SIZE 65536
#define MAX_EVENTS 10000

KHASH_MAP_INIT_STR(redis_hash, RedisValue *);
khash_t(redis_hash) * h;

void set_value(khash_t(redis_hash) * h, const char *key, const void *value, ValueType type) {
  int ret;
  // attempt to put the key into the hash table
  khiter_t k = kh_put(redis_hash, h, key, &ret);
  if (ret == 1) { // key not present
    kh_key(h, k) = strdup(key);
  } else { // key present, we're updating an existing entry
    // if the existing value is a string, free it
    if (kh_value(h, k)->type == TYPE_STRING) {
      free(kh_value(h, k)->data.str);
      free(kh_value(h, k));
    }
  }

  RedisValue *redis_value = malloc(sizeof(RedisValue));

  // set the type of the new value
  redis_value->type = type;
  kh_value(h, k) = redis_value;

  // handle the value based on its type
  if (type == TYPE_STRING) {
    kh_value(h, k)->data.str = strdup((char *)value);
  }
}

RedisValue *get_value(khash_t(redis_hash) * h, const char *key) {
  khiter_t k = kh_get(redis_hash, h, key);
  if (k != kh_end(h)) {
    return kh_value(h, k);
  }
  return NULL; // key not found
}

void cleanup_hash(khash_t(redis_hash) * h) {
  for (khiter_t k = kh_begin(h); k != kh_end(h); k++) {
    if (kh_exist(h, k)) {
      free((char *)kh_key(h, k)); // free the key
      RedisValue *rv = kh_value(h, k);
      if (rv->type == TYPE_STRING) {
        free(rv->data.str); // free the string data
      }
      free(rv);
    }
  }
  kh_destroy(redis_hash, h);
}

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
  if (ch->arg_count == 2) {
    add_simple_string_reply(ch->client, ch->args[1]);
  } else {
    add_error_reply(ch->client, "ERR wrong number of arguments for 'echo' command");
  }
}

void handle_set(CommandHandler *ch) {
  if (ch->arg_count > 2) {
    set_value(h, ch->args[1], ch->args[2], TYPE_STRING);
    add_simple_string_reply(ch->client, "OK");
  } else {
    add_error_reply(ch->client, "ERR wrong number of arguments for 'set' command");
  }
}

void handle_get(CommandHandler *ch) {
  RedisValue *redis_value = get_value(h, ch->args[1]);
  if (redis_value == NULL) {
    add_null_reply(ch->client);
  } else {
    add_bulk_string_reply(ch->client, redis_value->data.str); // Send the bulk string reply
  }
}

typedef enum { CMD_PING, CMD_ECHO, CMD_SET, CMD_GET, CMD_UNKNOWN } CommandType;

CommandType get_command_type(char *command) {
  if (strcmp(command, "PING") == 0)
    return CMD_PING;
  else if (strcmp(command, "ECHO") == 0)
    return CMD_ECHO;
  else if (strcmp(command, "SET") == 0)
    return CMD_SET;
  else if (strcmp(command, "GET") == 0)
    return CMD_GET;
  else
    return CMD_UNKNOWN;
}

void handle_command(CommandHandler *ch) {

  CommandType command_type = get_command_type(ch->args[0]);

  switch (command_type) {
  case CMD_PING:
    handle_ping(ch);
    break;
  case CMD_ECHO:
    handle_echo(ch);
    break;
  case CMD_SET:
    handle_set(ch);
    break;
  case CMD_GET:
    handle_get(ch);
    break;
  default:
    add_error_reply(ch->client, "ERR unknown command");
    break;
  }
}

void flush_output_buffer(Client *client) {
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

void client_free(Client *client) {
  close(client->fd);
  rb_destroy(client->input_buffer);
  free(client->parser);
  free(client);
}

void client_on_readable(Client *client) {
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
        flush_output_buffer(client);
        return;
      } else {
        perror("failed to read from client socket");
        return;
      }
    case 0:
      fprintf(stderr, "client disconnected\n");

      close(client->fd); // Close the socket
      destroy_command_handler(client->parser->command_handler);
      destroy_parser(client->parser);
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

    if (rb_read(client->input_buffer, bytes_parsed)) {
      fprintf(stderr, "failed to update read index\n");
      return;
    }

    if (bytes_received < writable_len) {
      flush_output_buffer(client);
      return;
    }
  }
}

volatile sig_atomic_t stop_server = 0;

void sigint_handler(int sig) { stop_server = 1; }

int start_server() {
  // register the signal handler
  signal(SIGINT, sigint_handler);

  // initialize the hash table
  h = kh_init(redis_hash);

  struct sockaddr_in sa;
  int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (SocketFD == -1) {
    perror("cannot create socket");
    exit(EXIT_FAILURE);
  }

  memset(&sa, 0, sizeof sa);

  sa.sin_family = AF_INET;
  sa.sin_port = htons(DEFAULT_PORT);
  sa.sin_addr.s_addr = htonl(INADDR_ANY);

  // set SO_REUSEADDR option
  int opt = 1;
  if (setsockopt(SocketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  if (bind(SocketFD, (struct sockaddr *)&sa, sizeof sa) == -1) {
    perror("bind failed");
    close(SocketFD);
    exit(EXIT_FAILURE);
  }

  if (listen(SocketFD, 10) == -1) {
    perror("listen failed");
    close(SocketFD);
    exit(EXIT_FAILURE);
  }

  printf("server started listening\n");

  int epfd = epoll_create(1);
  if (epfd == -1) {
    perror("epoll_create1 failed");
    exit(EXIT_FAILURE);
  }

  struct epoll_event event;
  event.events = EPOLLIN; // EPOLLIN is the flag for read events
  // event.data.fd = SocketFD;
  event.data.ptr = NULL;

  if (epoll_ctl(epfd, EPOLL_CTL_ADD, SocketFD, &event) == -1) {
    perror("epoll_ctl failed");
    exit(EXIT_FAILURE);
  }

  struct epoll_event events[MAX_EVENTS];
  int num_events;

  Handler *handler = create_handler();

  for (;;) {
    num_events = epoll_wait(epfd, events, MAX_EVENTS, -1);
    if (num_events == -1) {
      perror("epoll_wait failed");
      exit(EXIT_FAILURE);
    }

    // iterate through the events
    for (int i = 0; i < num_events; i++) {
      struct epoll_event *current_event = &events[i];
      if (!current_event->data.ptr) { // server socket is ready, new connection
        int ConnectFD = accept(SocketFD, NULL, NULL);
        if (ConnectFD == -1) {
          perror("accept failed");
          close(SocketFD);
          exit(EXIT_FAILURE);
        }

        Client *client = create_client(ConnectFD, handler);
        CommandHandler *command_handler = create_command_handler(client, 256, 10);
        parser_init(client->parser, handler, command_handler);

        int optval = 1;
        setsockopt(ConnectFD, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));

        event.events = EPOLLIN;
        event.data.fd = ConnectFD;
        event.data.ptr = client;

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, ConnectFD, &event) == -1) {
          perror("epoll_ctl failed");
          exit(EXIT_FAILURE);
        }
      } else if (events[i].events & EPOLLIN) {
        Client *client = (Client *)current_event->data.ptr;
        client_on_readable(client);
      } else if (events[i].events & EPOLLHUP | EPOLLERR) {
        Client *client = (Client *)current_event->data.ptr;
        destroy_command_handler(client->parser->command_handler);
        destroy_parser(client->parser);
        client_free(current_event->data.ptr);
        epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
      } else {
        fprintf(stderr, "Unexpected event type: %d\n", events[i].events);
      }

      if (stop_server) {
        break;
      }
    }
  }
  close(epfd);
  close(SocketFD);
  cleanup_hash(h);
  destroy_handler(handler);
  return EXIT_SUCCESS;
}
