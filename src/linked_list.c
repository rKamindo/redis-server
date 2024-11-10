#include "linked_list.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
  char *data;
  struct Node *prev;
  struct Node *next;
} Node;

struct list_struct {
  Node *head;
  Node *tail;
  size_t length;
};

List create_list() {
  List list = malloc(sizeof(struct list_struct));
  if (!list) {
    return NULL;
  }
  list->head = NULL;
  list->tail = NULL;
  return list;
}

void destroy_list(List list) {
  if (list == NULL) {
    return;
  }

  Node *cur = list->head;
  while (cur != NULL) {
    Node *temp = cur->next;
    cur->prev = NULL;
    cur->next = NULL;
    free(cur->data);
    free(cur);
    cur = temp;
  }
  free(list);
}

int lpush(List list, const char *data, int *length) {
  if (list == NULL || data == NULL) {
    return 0;
  }

  Node *new_node = malloc(sizeof(Node));
  if (new_node == NULL) {
    return 0;
  }
  new_node->data = strdup(data);
  if (new_node->data == NULL) {
    free(new_node); // clean up allocated node if strdup fails
    return 0;
  }

  // list is empty
  if (list->head == NULL && list->tail == NULL) {
    list->head = new_node;
    list->tail = new_node;
    new_node->prev = NULL;
    new_node->next = NULL;
    list->length = 1;
    *length = list->length;
    return 0;
  }

  new_node->next = list->head;
  new_node->prev = NULL;
  list->head->prev = new_node;
  list->head = new_node;
  list->length++;
  *length = list->length;
  return 0;
}

int rpush(List list, const char *data, int *length) {
  if (list == NULL || data == NULL) {
    return 0;
  }

  Node *new_node = malloc(sizeof(Node));
  if (new_node == NULL) {
    return 0;
  }
  new_node->data = strdup(data);
  if (new_node->data == NULL) {
    free(new_node); // clean up allocated node if strdup fails
    return 0;
  }

  // list is empty
  if (list->head == NULL && list->tail == NULL) {
    list->head = new_node;
    list->tail = new_node;
    new_node->prev = NULL;
    new_node->next = NULL;
    list->length = 1;
    *length = list->length;
    return 0;
  }

  new_node->prev = list->tail;
  new_node->next = NULL;
  list->tail->next = new_node;
  list->tail = new_node;
  list->length++;
  *length = list->length;
  return 0;
}

char **lrange(List list, int start, int end, int *range_length) {
  if (list == NULL) {
    *range_length = 0;
    return NULL;
  }

  // handle negative start value,
  if (start < 0) {
    start = list->length + start;
  }

  // handle negative end value
  if (end < 0) {
    end = list->length + end;
  }

  if (start < 0 || start >= list->length || end < start) {
    *range_length = 0;
    return NULL;
  }

  // adjust end if it's beyond the list length, list is "0-indexed"
  if (end >= list->length) {
    end = list->length - 1;
  }

  int count = end - start + 1;
  *range_length = count;

  char **result = malloc(count * sizeof(char *));
  if (result == NULL) {
    *range_length = 0;
    return NULL;
  }

  Node *cur = list->head;
  for (int i = 0; i < start && cur != NULL; i++) {
    cur = cur->next;
  }

  int i;
  for (i = 0; i < count && cur != NULL; i++) {
    result[i] = strdup(cur->data);
    if (result[i] == NULL) {
      // clean up on allocation failure
      for (int j = 0; j < i; j++) {
        free(result[j]);
      }
      return NULL;
    }
    cur = cur->next;
  }

  // check if we reached the end of the list before filling the entire range
  if (cur == NULL && i < count) {
    *range_length = i; // adjust the range length to the actual number of elements copied
  }

  return result;
}

size_t get_list_length(List list) {
  if (list == NULL) {
    return 0;
  }
  return list->length;
}

void cleanup_lrange_result(char **range, int range_length) {
  for (int i = 0; i < range_length; i++) {
    free(range[i]);
  }
}
