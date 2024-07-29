#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Serialization functions

// Serialize a simple string
char *serialize_simple_string(const char *str) {
  // check for null input
  if (str == NULL) {
    return NULL;
  }

  // calculate the length of the input string
  size_t len = strlen(str);

  // allocate memory for the serialized string
  // length = 1 (for '+') + len (for the string) + 2 (for "\r\n") + 1 for null
  // terminator
  char *serialized = (char *)malloc(len + 4);
  if (serialized == NULL) {
    return NULL;  // memory allocation failed
  }

  // format the serialized string
  snprintf(serialized, len + 4, "+%s\r\n", str);

  return serialized;
}

char *serialize_error(const char *str) {
  if (str == NULL) {
    return NULL;
  }

  size_t len = strlen(str);

  char *serialized = (char *)malloc(len + 4);
  if (serialized == NULL) {
    return NULL;
  }

  snprintf(serialized, len + 4, "-%s\r\n", str);
  return serialized;
}

char *serialize_integer(const int val) {
  // calculate the length of the integer
  int len = snprintf(NULL, 0, "%d", val);

  char *serialized = (char *)malloc(len + 4);
  if (serialized == NULL) {
    return NULL;
  }

  snprintf(serialized, len + 4, ":%d\r\n", val);
  return serialized;
}

char *serialize_bulk_string(const char *str) {
  if (str == NULL) {
    return _strdup("$-1\r\n");  // null representation of bulk string
  }

  size_t str_len = strlen(str);

  // length of length prefix
  int prefix_len = snprintf(NULL, 0, "%zu", str_len);

  // total length
  // 1 ('%s') + prefix_len + 2 ("\r\n") + str_len + 2 ("\r\n") + 1 ("\0")
  size_t total_len = prefix_len + str_len + 6;

  char *serialized = (char *)malloc(total_len);
  if (serialized == NULL) {
    return NULL;
  }

  snprintf(serialized, total_len, "$%zu\r\n%s\r\n", str_len, str);
  return serialized;
}

char *serialize_array(const char **arr, int count) {
  if (arr == NULL) {
    return _strdup("*-1\r\n");
  }

  // total length for the serialized array
  size_t total_len = 1 + snprintf(NULL, 0, "%d", count) + 2;
  for (int i = 0; i < count; i++) {
    const char *str = arr[i];
    if (str == NULL) {
      total_len += 5;  // for "$-1\r\n"
    } else {
      size_t str_len = strlen(str);
      // 1 for "$", length of str_len, 2 for "\r\n", str_len, 2 for last "\r\n"
      total_len += 1 + snprintf(NULL, 0, "%zu", str_len) + 2 + str_len + 2;
    }
  }

  // allocate memory for the serialized array
  char *serialized = (char *)malloc(total_len + 1);  // + 1 for null terminator
  if (serialized == NULL) {
    return NULL;
  }

  // serialize
  char *current = serialized;

  current += sprintf(current, "*%d\r\n", count);
  for (int i = 0; i < count; i++) {
    const char *str = arr[i];
    if (str == NULL) {
      memcpy(current, "$-1\r\n", 5);
      current += 5;
    } else {
      char *serialized_element = serialize_bulk_string(arr[i]);
      size_t elem_len = strlen(serialized_element);
      memcpy(current, serialized_element, elem_len);
      current += elem_len;
      free(serialized_element);
    }
  }

  *current = '\0';  // null terminate the string
  return serialized;
}

// "*2\r\n$4\r\necho\r\n$11\r\nhello world\r\n”
// a command is an array of bulk strings
char **deserialize_command(const char *input, int *count) {
  if (input == NULL || input[0] != '*') {
    *count = 0;
    return NULL;  // not an array, invalid command
  }

  // parse the number of elements in the array
  *count = atoi(input + 1);
  char **result = (char **)malloc(*count * sizeof(char *));
  if (result == NULL) {
    return NULL;
  }

  const char *current = strchr(input, '\n') + 1;
  for (int i = 0; i < *count; i++) {
    if (current[0] != '$') {
      // expected a bulk string, but did not find one
      for (int j = 0; j < i; j++) free(result[j]);
      free(result);
      *count = 0;
      return NULL;
    }

    // parse the length of the bulk string
    int len = atoi(current + 1);
    current = strchr(current, '\n') + 1;

    if (len == -1) {
      // this is a null bulk string
      result[i] = NULL;
    } else {
      // allocate memory for this string and copy it
      result[i] = (char *)malloc(len + 1);
      if (result[i] == NULL) {
        for (int j = 0; j < i; j++) free(result[j]);
        free(result);
        *count = 0;
        return NULL;
      }
      strncpy(result[i], current, len);
      result[i][len] = '\0';  // null terminate

      // move on to the next element
      current = strchr(current, '\n') + 1;
    }
  }

  return result;
}

void free_command(char **command, int count) {
  for (int i = 0; i < count; i++) {
    free(command[i]);
  }
  free(command);
}