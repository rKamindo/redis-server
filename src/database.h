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

typedef struct redis_db {
  khash_t(redis_hash) * h;
  size_t key_count;
  size_t expiry_count;
} redis_db_t;

redis_db_t *redis_db_create();
void redis_db_destroy(redis_db_t *db);
void redis_db_set(redis_db_t *db, const char *key, const void *value, ValueType type,
                  long long expiration);
RedisValue *redis_db_get(redis_db_t *db, const char *key);
bool redis_db_exist(redis_db_t *db, const char *key);
void redis_db_delete(redis_db_t *db, const char *key);
int redis_db_lpush(redis_db_t *db, const char *key, const char *item, int *length);
int redis_db_rpush(redis_db_t *db, const char *key, const char *item, int *length);
int redis_db_lrange(redis_db_t *db, const char *key, int start, int end, char ***range,
                    int *range_length);
bool redis_db_save(redis_db_t *db);
size_t redis_db_dbsize(redis_db_t *db);
size_t redis_db_expiry_count(redis_db_t *db);
#endif // DATABASE_H