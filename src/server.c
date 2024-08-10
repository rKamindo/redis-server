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

#include "khash.h"
#include "resp.h"

#define DEFAULT_PORT 6379
#define BUFFER_SIZE 1024
#define MAX_EVENTS 50

typedef enum { TYPE_STRING } ValueType;

typedef struct {
  ValueType type;
  union {
    char *str;
  } data;
} RedisValue;

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
      free((char *)kh_key(h, k));
      if (kh_value(h, k)->type == TYPE_STRING) {
        free(kh_value(h, k)->data.str);
      }
    }
  }
  kh_destroy(redis_hash, h);
}

/**
 * Sends a response to the client and optionally frees the response memory.
 */
void add_reply(int ConnectFD, const char *response, int free_response) {
  if (response) {
    ssize_t bytes_sent = send(ConnectFD, response, strlen(response), 0);
    if (bytes_sent < 0) {
      perror("send failed");
    } else {
      printf("Sent: %s\n", response);
    }
    if (free_response) {
      free((void *)response);
    }
  }
}

void handle_echo(int ConnectFD, char *response) {
  add_reply(ConnectFD, serialize_bulk_string(response), 1);
}

void handle_set(int ConnectFD, char *key, char *value) {
  set_value(h, key, value, TYPE_STRING);
  add_reply(ConnectFD, "+OK\r\n", 0);
}

void handle_get(int ConnectFD, char *key) {
  RedisValue *redis_value = get_value(h, key);
  if (redis_value == NULL) {
    add_reply(ConnectFD, "$-1\r\n", 0);
  } else {
    add_reply(ConnectFD, serialize_bulk_string(redis_value->data.str), 1);
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

void handle_command(int ConnectFD, char **parsed_command, int count) {
  CommandType command_type = get_command_type(parsed_command[0]);
  switch (command_type) {
  case CMD_PING:
    add_reply(ConnectFD, "+PONG\r\n", 0);
    break;
  case CMD_ECHO:
    if (count > 1) handle_echo(ConnectFD, parsed_command[1]);
    break;
  case CMD_SET:
    if (count > 2) {
      char *key = parsed_command[1];
      char *value = parsed_command[2];
      handle_set(ConnectFD, key, value);
    }
    break;
  case CMD_GET:
    if (count > 1) handle_get(ConnectFD, parsed_command[1]);
    break;
  default:
    add_reply(ConnectFD, serialize_error("ERR unknown command"), 1);
    break;
  }
  free_command(parsed_command, count);
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
  event.data.fd = SocketFD;

  if (epoll_ctl(epfd, EPOLL_CTL_ADD, SocketFD, &event) == 1) {
    perror("epoll_ctl failed");
    exit(EXIT_FAILURE);
  }

  struct epoll_event events[MAX_EVENTS];
  int num_events;

  for (;;) {
    num_events = epoll_wait(epfd, events, MAX_EVENTS, -1);
    if (num_events == -1) {
      perror("epoll_wait failed");
      exit(EXIT_FAILURE);
    }

    // iterate through the events
    for (int i = 0; i < num_events; i++) {
      if (events[i].data.fd == SocketFD) { // server socket is ready, new connection
        int ConnectFD = accept(SocketFD, NULL, NULL);
        if (ConnectFD == -1) {
          perror("accept failed");
          close(SocketFD);
          exit(EXIT_FAILURE);
        }

        int optval = 1;
        setsockopt(ConnectFD, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));

        event.events = EPOLLIN;
        event.data.fd = ConnectFD;

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, ConnectFD, &event) == -1) {
          perror("epoll_ctl failed");
          exit(EXIT_FAILURE);
        }
      } else {
        char buffer[BUFFER_SIZE];
        ssize_t bytes_received;

        // read from the client
        bytes_received = recv(events[i].data.fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
          buffer[bytes_received] = '\0'; // null terminate the received string
          printf("Received: %s\n", buffer);
          int count;
          char **parsed_command = deserialize_command(buffer, &count);
          if (parsed_command && count > 0) {
            handle_command(events[i].data.fd, parsed_command, count); // handle the command
          }
        } else if (bytes_received == 0) {
          printf("Client disconnected\n");
          close(events[i].data.fd);
          epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
        } else {
          printf("read failed");
          close(events[i].data.fd);
          epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
        }
      }
    }
    if (stop_server) {
      break;
    }
  }
  close(epfd);
  close(SocketFD);
  cleanup_hash(h);
  return EXIT_SUCCESS;
}
