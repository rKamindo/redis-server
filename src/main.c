#include "redis-server.h"
#include <stdio.h>

int main(int argc, char **argv) {
  printf("# redis_lite started\n");
  int status = start_server(argc, argv);
  return status;
}