#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

typedef struct server_config {
  char dir[256];
  char dbfilename[256];
} server_config_t;

typedef struct server_info {
  char role[16];
} server_info_t;

extern server_config_t g_server_config;
extern server_info_t g_server_info;

#endif // SERVER_CONFIG_H