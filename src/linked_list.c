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

size_t get_list_length(List list) {
  if (list == NULL) {
    return 0;
  }
  return list->length;
}
