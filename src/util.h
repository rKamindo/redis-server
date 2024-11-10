#ifndef UTIL_H
#define UTIL_H

#include <errno.h>
#include <stdlib.h>

#define ERR_NONE 0
#define ERR_SYNTAX -1
#define ERR_VALUE -2
#define ERR_TYPE_MISMATCH -3

long long current_time_millis();
int parse_integer(const char *str, long *result);
#endif // UTIL_H