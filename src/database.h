#ifndef DATABASE_H
#define DATABASE_H
#include "khash.h"
#include "sys/time.h"

typedef enum { TYPE_STRING } ValueType;

typedef struct {
  ValueType type;
  union {
    char *str;
  } data;
  time_t expiration;
} RedisValue;

KHASH_MAP_INIT_STR(redis_hash, RedisValue *)

void redis_db_create();
void redis_db_destroy();
void redis_db_set(const char *key, const char *value, ValueType type, long long expiration);
RedisValue *redis_db_get(const char *key);
long long current_time_millis();

#endif // DATABASE_H