#ifndef SERVER_H
#define SERVER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "client.h"
#include "command_handler.h"
#include "khash.h"
#include "resp.h"

// foward declarations
struct CommandHandler;

void handle_client_disconnection(Client *client, int epfd);

int start_server();

#ifdef __cplusplus
}
#endif

#endif // SERVER_H