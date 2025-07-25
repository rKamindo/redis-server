#ifndef UTIL_H
#define UTIL_H

#include <errno.h>
#include <stdlib.h>

#define ERR_NONE 0
#define ERR_SYNTAX -1
#define ERR_VALUE -2
#define ERR_TYPE_MISMATCH -3
#define ERR_KEY_NOT_FOUND -4

long long current_time_millis();
int parse_integer(const char *str, long *result);
char *construct_file_path(const char *dir, const char *filename);
void set_non_blocking(int fd);
#endif // UTIL_H