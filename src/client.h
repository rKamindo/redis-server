#ifndef CLIENT_H
#define CLIENT_H

#include "resp.h"        // Include this for the Parser definition
#include "ring_buffer.h" // Include this if ring_buffer is defined in a separate header

struct Parser;

typedef struct Client {
  int fd;
  ring_buffer input_buffer;
  ring_buffer output_buffer;
  struct Parser *parser;
} Client;

#endif // CLIENT_H