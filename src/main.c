#include "redis-server.h"

int main(int argc, char **argv) {
  int status = start_server(argc, argv);
  return status;
}