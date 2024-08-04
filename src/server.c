#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "khash.h"
#include "resp.h"

#define DEFAULT_PORT 6379
#define BUFFER_SIZE 1024

typedef enum { TYPE_STRING } ValueType;

typedef struct {
  ValueType type;
  union {
    char *str;
  } data;
} RedisValue;

KHASH_MAP_INIT_STR(redis_hash, RedisValue);

void set_value(khash_t(redis_hash) * h, const char *key, const void *value,
               ValueType type) {
  int ret;
  // attempt to put the key into the hash table
  khiter_t k = kh_put(redis_hash, h, key, &ret);
  if (ret == 1) {  // key not present
    kh_key(h, k) = strdup(key);
  } else {  // key present, we're updating an existing entry
    // if the existing value is a string, free it
    if (kh_value(h, k).type == TYPE_STRING) {
      free(kh_value(h, k).data.str);
    }
  }

  // set the type of the new value
  kh_value(h, k).type = type;

  // handle the value based on its type
  if (type == TYPE_STRING) {
    kh_value(h, k).data.str = strdup((char *)value);
  }
}

RedisValue *get_value(khash_t(redis_hash) * h, const char *key) {
  khiter_t k = kh_get(redis_hash, h, key);
  if (k != kh_end(h)) {
    return &kh_value(h, k);
  }
  return NULL;  // key not found
}

void cleanup_hash(khash_t(redis_hash) * h) {
  for (khiter_t k = kh_begin(h); k != kh_end(h); k++) {
    if (kh_exist(h, k)) {
      free((char *)kh_key(h, k));
      if (kh_value(h, k).type == TYPE_STRING) {
        free(kh_value(h, k).data.str);
      }
    }
  }
  kh_destroy(redis_hash, h);
}

int start_server() {
  // initialize the hash table
  khash_t(redis_hash) *h = kh_init(redis_hash);

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

  printf("server started listening");

  for (;;) {
    int ConnectFD = accept(SocketFD, NULL, NULL);

    if (ConnectFD == -1) {
      perror("accept failed");
      close(SocketFD);
      exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    // read from the client
    bytes_received = recv(ConnectFD, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
      buffer[bytes_received] = '\0';  // null terminate the received string
      printf("Received: %s\n", buffer);

      // deserialize received RESP data
      int count;
      char **parsed_command = deserialize_command(buffer, &count);
      if (parsed_command && count > 0) {
        // check if the received message is ping
        if (strcmp(parsed_command[0], "PING") == 0) {
          const char *response = serialize_simple_string("PONG");
          ssize_t bytes_sent = send(ConnectFD, response, strlen(response), 0);
          if (bytes_sent < 0) {
            perror("send failed");
          } else {
            printf("Sent: %s\n", response);
          }
        } else if (strcmp(parsed_command[0], "ECHO") == 0 && count > 1) {
          const char *response = serialize_bulk_string(parsed_command[1]);
          ssize_t bytes_sent = send(ConnectFD, response, strlen(response), 0);
          if (bytes_sent < 0) {
            perror("send failed");
          } else {
            printf("Sent: %s\n", response);
          }
        } else if (strcmp(parsed_command[0], "SET") == 0 && count > 2) {
          set_value(h, parsed_command[1], parsed_command[2], TYPE_STRING);
          const char *response = serialize_simple_string("OK");
          ssize_t bytes_sent = send(ConnectFD, response, strlen(response), 0);
          if (bytes_sent < 0) {
            perror("send failed");
          } else {
            printf("Sent: %s\n", response);
          }
        } else if (strcmp(parsed_command[0], "GET") == 0 && count > 1) {
          RedisValue *redis_value = get_value(h, parsed_command[1]);
          const char *response;
          if (redis_value == NULL) {
            response = serialize_bulk_string(NULL);
          } else {
            const char *response = serialize_bulk_string(redis_value->data.str);
          }
          ssize_t bytes_sent = send(ConnectFD, response, strlen(response), 0);
          if (bytes_sent < 0) {
            perror("send failed");
          } else {
            printf("Sent: %s\n", response);
          }
        }
        // free the deserialized command
        free_command(parsed_command, count);
      } else {
        printf("Invalid command received\n");
      }
    } else if (bytes_received == 0) {
      printf("Client disconnected\n");
    } else {
      printf("read failed");
    }

    // if (shutdown(ConnectFD, SHUT_RDWR) == -1) {
    //   perror("shutdown failed");
    // }
    close(ConnectFD);
  }

  close(SocketFD);
  cleanup_hash(h);
  return EXIT_SUCCESS;
}