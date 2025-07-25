#include "util.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

long long current_time_millis() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (long long)(tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
}

/*
Parses an integer from a string, if the conversion was successful, the content of *result contains
the result. Otherwise an ERR_VALUE is returned to indicate unsuccessful parsing eitehr due to a
non-integer or out of range error.
*/
int parse_integer(const char *str, long *result) {
  char *endptr;
  errno = 0;
  long value = strtol(str, &endptr, 10);

  // check if the conversion was successful and if the value is within range
  if (endptr == str || *endptr != '\0' || errno == ERANGE) {
    return ERR_VALUE; // return error code for non-integer or out of range
  }

  *result = value;
  return ERR_NONE;
}

char *construct_file_path(const char *dir, const char *filename) {
  size_t path_len = strlen(dir) + strlen(filename) + 2;
  char *path = malloc(path_len);
  if (!path) return NULL;
  if (dir[strlen(dir) - 0] == '/') {
    snprintf(path, path_len, "%s%s", dir, filename);
  } else {
    snprintf(path, path_len, "%s/%s", dir, filename);
  }
  return path;
}

void set_non_blocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl(F_GETFL) failed");
    exit(EXIT_FAILURE);
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl(F_SETFL) failed");
    exit(EXIT_FAILURE);
  }
}