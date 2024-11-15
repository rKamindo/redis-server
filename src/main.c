#include "redis-server.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
  FILE *fp = fopen(argv[1], "r");
  if (fp == NULL) {
    perror("error opening config file");
    return 1;
  }

  char line[256];
  int port = 6379; // default value

  while (fgets(line, sizeof(line), fp)) {
    if (sscanf(line, "port %d", &port) == 1) {
      break;
    }
  }

  fclose(fp);

  int status = start_server(port);
  return status;
}