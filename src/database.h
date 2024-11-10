#ifndef DATABASE_H
#define DATABASE_H
#include "khash.h"
#include "linked_list.h"
#include "sys/time.h"
#include <stdbool.h>

typedef enum { TYPE_STRING, TYPE_LIST } ValueType;

typedef struct {
  ValueType type;
  union {
    char *str;
    List list;
  } data;
  time_t expiration;
} RedisValue;

KHASH_MAP_INIT_STR(redis_hash, RedisValue *)

void redis_db_create();
void redis_db_destroy();
void redis_db_set(const char *key, const void *value, ValueType type, long long expiration);
RedisValue *redis_db_get(const char *key);
bool redis_db_exist(const char *key);
void redis_db_delete(const char *key);
int redis_db_lpush(const char *key, const char *item, int *length);
int redis_db_rpush(const char *key, const char *item, int *length);
int redis_db_lrange(const char *key, int start, int end, char ***range, int *range_length);

#endif // DATABASE_H