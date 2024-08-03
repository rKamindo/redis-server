#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "resp.h"

#define DEFAULT_PORT 6379
#define BUFFER_SIZE 1024

int main() {
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

    if (shutdown(ConnectFD, SHUT_RDWR) == -1) {
      perror("shutdown failed");
    }
    close(ConnectFD);
  }

  close(SocketFD);
  return EXIT_SUCCESS;
}