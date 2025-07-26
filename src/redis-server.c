#include "redis-server.h"
#include "arpa/inet.h"
#include "client.h"
#include "command_handler.h"
#include "commands.h"
#include "database.h"
#include "rdb.h"
#include "replication.h"
#include "resp.h"
#include "ring_buffer.h"
#include "server_config.h"
#include "util.h"
#include <errno.h>
#include <netdb.h>
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

#define DEFAULT_PORT 6379
#define MAX_EVENTS 10000
#define MAX_PATH_LENGTH 256

server_config_t g_server_config = {.dir = "/tmp/redis-data", .dbfilename = "dump.rdb"};

server_info_t g_server_info = {.role = "master",
                               .master_replid =
                                   "8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb", // hard code for now
                               .master_repl_offset = 0};

int g_epoll_fd; // epollfd global
int port = DEFAULT_PORT;
volatile sig_atomic_t stop_server = 0;

void sigint_handler(int sig) { stop_server = 1; }

int start_server(int argc, char *argv[]) {
  g_handler = create_handler();
  redis_db_t *db = redis_db_create();
  g_epoll_fd = epoll_create(1);
  if (g_epoll_fd == -1) {
    perror("epoll_create failed");
    exit(EXIT_FAILURE);
  }

  // parse command-line args
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--dir") == 0) {
      if (i + 1 < argc) {
        snprintf(g_server_config.dir, MAX_PATH_LENGTH, "%s", argv[i + 1]);
        i++;
      }
    } else if (strcmp(argv[i], "--dbfilename") == 0) {
      if (i + 1 < argc) {
        snprintf(g_server_config.dbfilename, MAX_PATH_LENGTH, "%s", argv[i + 1]);
        i++;
      }
    } else if (strcmp(argv[i], "--port") == 0) {
      if (i + 1 < argc) {
        strcpy(g_server_config.port, argv[i + 1]);
        port = atoi(argv[i + 1]);
        i++;
      }
    } else if (strcmp(argv[i], "--replicaof") == 0) {
      if (i + 2 < argc) {
        strcpy(g_server_info.role, "slave");
        strcpy(g_server_config.master_host, argv[i + 1]);
        strcpy(g_server_config.master_port, argv[i + 2]);
        i += 2;
      }
    }
  }

  if (strcmp(g_server_info.role, "slave") == 0) {
    int master_fd;
    char port_str[6];

    // resolve address
    struct addrinfo hints, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    int status =
        getaddrinfo(g_server_config.master_host, g_server_config.master_port, &hints, &res);
    if (status != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
      exit(EXIT_FAILURE);
    }

    // loop through all the results and connect to the first one we can
    bool connected = false;
    struct addrinfo *p;
    for (p = res; p != NULL; p = p->ai_next) {

      // create socket
      master_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if (master_fd == -1) {
        perror("cannot create socket for master connection");
        exit(EXIT_FAILURE);
      }

      // connect to master
      if (connect(master_fd, p->ai_addr, p->ai_addrlen) == -1) {
        close(master_fd);
        continue;
      }

      connected = true;
      break;
    }

    freeaddrinfo(res);

    if (!connected) {
      perror("Replica: failed to connect to master...\n");
      exit(EXIT_FAILURE);
    }

    Client *master_client = create_client(master_fd);
    set_non_blocking(master_fd);
    master_client->db = db;
    master_client->type = CLIENT_TYPE_MASTER;
    master_client->repl_client_state = REPL_STATE_CONNECTING;

    CommandHandler *command_handler = create_command_handler(master_client, 256, 10);
    parser_init(master_client->parser, command_handler);

    struct epoll_event event;
    event.events = EPOLLIN | EPOLLOUT;
    event.data.fd = master_fd;
    event.data.ptr = master_client;

    if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, master_fd, &event) == -1) {
      perror("epoll_ctl for master_fd failed");
      exit(EXIT_FAILURE);
    }
  }

  // register the signal handler
  signal(SIGINT, sigint_handler);

  if (strcmp(g_server_info.role, "slave") != 0) {
    // for now only load file if it is not a replica
    rdb_load_data_from_file(db, g_server_config.dir, g_server_config.dbfilename);
  }

  struct sockaddr_in sa;
  int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (SocketFD == -1) {
    perror("cannot create socket");
    exit(EXIT_FAILURE);
  }

  memset(&sa, 0, sizeof sa);

  const char *bind_address = "127.0.0.1";

  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  inet_pton(AF_INET, bind_address, &(sa.sin_addr));

  // set SO_REUSEADDR option
  int opt = 1;
  if (setsockopt(SocketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  printf("# Creating Server TCP listening socket %s:%d\n", bind_address, ntohs(sa.sin_port));

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

  set_non_blocking(SocketFD);

  struct epoll_event event;
  event.events = EPOLLIN; // EPOLLIN is the flag for read events
  event.data.ptr = NULL;

  if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, SocketFD, &event) == -1) {
    perror("epoll_ctl failed");
    exit(EXIT_FAILURE);
  }

  struct epoll_event events[MAX_EVENTS];
  int num_events;

  printf("# Ready to accept connections\n");
  for (;;) {
    num_events = epoll_wait(g_epoll_fd, events, MAX_EVENTS, -1);
    if (num_events == -1 && errno != EINTR) {
      perror("epoll_wait failed");
      exit(EXIT_FAILURE);
    }

    // iterate through the events
    for (int i = 0; i < num_events; i++) {
      struct epoll_event *current_event = &events[i];
      Client *client = (Client *)current_event->data.ptr;
      if (!current_event->data.ptr) { // server socket is ready, new connection
        int ConnectFD = accept(SocketFD, NULL, NULL);
        if (ConnectFD == -1) {
          perror("accept failed");
          close(SocketFD);
          exit(EXIT_FAILURE);
        }

        set_non_blocking(ConnectFD);

        Client *new_client = create_client(ConnectFD);

        new_client->type = CLIENT_TYPE_REGULAR;
        select_client_db(new_client, db);
        CommandHandler *command_handler = create_command_handler(new_client, 256, 10);
        parser_init(new_client->parser, command_handler);

        int optval = 1;
        setsockopt(ConnectFD, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));

        event.events = EPOLLIN;
        new_client->epoll_events = event.events;
        event.data.fd = ConnectFD;
        event.data.ptr = new_client;

        if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, ConnectFD, &event) == -1) {
          perror("epoll_ctl failed");
          exit(EXIT_FAILURE);
        }
      } else if (events[i].events & EPOLLIN) {
        process_client_input(client);
      } else if (events[i].events & EPOLLOUT) {
        if (client->type == CLIENT_TYPE_REPLICA) {
          master_handle_replica_out(client);
        } else if (client->type == CLIENT_TYPE_MASTER) {
          replica_handle_master_data(client);
        }
      } else if (events[i].events & EPOLLHUP | EPOLLERR) {
        handle_client_disconnection(client);
      } else {
        fprintf(stderr, "Unexpected event type: %d\n", events[i].events);
      }
    }

    if (stop_server) {
      printf("# User requested shutdown...\n");
      break;
    }
  }
  close(g_epoll_fd);
  close(SocketFD);
  // saves the currently selected db
  // TODO when we support multiple databases, save all of the databases
  printf("# Saving the final RDB snapshot before exiting.\n");
  if (redis_db_save(db)) {
    printf("# DB saved on disk\n");
  }
  redis_db_destroy(db);
  destroy_handler(g_handler);
  printf("# redis_lite is now ready to exit, bye bye...\n");
  return EXIT_SUCCESS;
}