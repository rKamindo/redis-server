#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "client.h"
#include "command_handler.h"
#include "database.h"
#include "redis-server.h"
#include "resp.h"
#include <errno.h>

#define DEFAULT_PORT 6379
#define MAX_EVENTS 10000

void process_client_input(Client *client, int epfd) {
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
      handle_client_disconnection(client, epfd);
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

void handle_client_disconnection(Client *client, int epfd) {
  destroy_client(client);
  epoll_ctl(epfd, EPOLL_CTL_DEL, client->fd, NULL);
}

volatile sig_atomic_t stop_server = 0;

void sigint_handler(int sig) { stop_server = 1; }

int start_server() {
  // register the signal handler
  signal(SIGINT, sigint_handler);

  redis_db_create();

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
        process_client_input(client, epfd);
      } else if (events[i].events & EPOLLHUP | EPOLLERR) {
        Client *client = (Client *)current_event->data.ptr;
        handle_client_disconnection(client, epfd);
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
  redis_db_destroy();
  destroy_handler(handler);
  return EXIT_SUCCESS;
}
