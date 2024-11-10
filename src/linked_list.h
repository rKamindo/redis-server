#ifndef LINKED_LIST_H
#define LINKED_LIST_H
#include "stddef.h"

struct list_struct;
typedef struct list_struct *List;

List create_list();
void destroy_list(List list);
int lpush(List list, const char *data, int *length);
int rpush(List list, const char *data, int *length);
char **lrange(List list, int start, int end, int *range_length);
size_t get_list_length(List list);

#endif // LINKED_LIST_H