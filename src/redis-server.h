#ifndef SERVER_H
#define SERVER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "client.h"
#include "command_handler.h"
#include "khash.h"
#include "resp.h"
#include <sys/epoll.h>

// foward declarations
struct CommandHandler;

void handle_client_disconnection(Client *client);

int start_server();

extern int g_epoll_fd; // global epoll fd
void client_enable_write_events(Client *client);
void client_disable_write_events(Client *client);

#ifdef __cplusplus
}
#endif

#endif // SERVER_H