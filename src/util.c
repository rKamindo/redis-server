#include "util.h"
#include <errno.h>
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
  if (endptr == str || *endptr != '\0' || errno == ERANGE || value < 0) {
    return ERR_VALUE; // return error code for non-integer or out of range
  }

  *result = value;
  return ERR_NONE;
}