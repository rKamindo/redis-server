#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H
#include "ring_buffer.h"

#define MAX_REPLICAS 16

typedef struct server_config {
  char dir[256];
  char dbfilename[256];
  char port[6];
  char master_host[128];
  char master_port[6];
} server_config_t;

typedef enum { ROLE_MASTER, ROLE_SLAVE } server_role_t;
typedef struct server_info {
  server_role_t role;
  char master_replid[40];
  long long master_repl_offset;
  ring_buffer repl_backlog;
  long long repl_backlog_base_offset;
  // fields for replica management
  Client *replicas[MAX_REPLICAS];
  size_t num_replicas;
} server_info_t;

extern server_config_t g_server_config;
extern server_info_t g_server_info;

#endif // SERVER_CONFIG_H