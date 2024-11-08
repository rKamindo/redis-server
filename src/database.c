#include "database.h"
#include "khash.h"

khash_t(redis_hash) * h;

long long current_time_millis() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (long long)(tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
}

void create_redis_hash_table() {
  // initialize the hash table
  h = kh_init(redis_hash);
}

void destroy_redis_hash(khash_t(redis_hash) * h) {
  for (khiter_t k = kh_begin(h); k != kh_end(h); k++) {
    if (kh_exist(h, k)) {
      free((char *)kh_key(h, k)); // free the key
      RedisValue *rv = kh_value(h, k);
      if (rv->type == TYPE_STRING) {
        free(rv->data.str); // free the string data
      }
      free(rv);
    }
  }
  kh_destroy(redis_hash, h);
}

void set_value(khash_t(redis_hash) * h, const char *key, const void *value, ValueType type,
               long long expiration) {
  int ret;
  // attempt to put the key into the hash table
  khiter_t k = kh_put(redis_hash, h, key, &ret);
  if (ret == 1) { // key not present
    kh_key(h, k) = strdup(key);
  } else { // key present, we're updating an existing entry
    // if the existing value is a string, free it
    RedisValue *old_value = kh_value(h, k);
    if (old_value->type == TYPE_STRING) {
      free(old_value->data.str);
    }
    free(old_value);
  }

  RedisValue *redis_value = malloc(sizeof(RedisValue));
  redis_value->type = type;
  redis_value->expiration = expiration;

  // handle the value based on its type
  if (type == TYPE_STRING) {
    redis_value->data.str = strdup((char *)value);
  }

  kh_value(h, k) = redis_value;
}

RedisValue *get_value(khash_t(redis_hash) * h, const char *key) {
  khiter_t k = kh_get(redis_hash, h, key);
  if (k != kh_end(h)) {
    RedisValue *value = kh_value(h, k);
    if (value->expiration > 0 && value->expiration < current_time_millis()) {
      // key value has expired, remove it
      if (value->type == TYPE_STRING) {
        free(value->data.str);
      }
      free(value);
      kh_del(redis_hash, h, k);
      return NULL;
    }
    return value;
  }
  return NULL; // key not found
}

void redis_db_create() { create_redis_hash_table(); }
void redis_db_destroy() { destroy_redis_hash(h); }

void redis_db_set(const char *key, const char *value, ValueType type, long long expiration) {
  set_value(h, key, value, type, expiration);
}

RedisValue *redis_db_get(const char *key) { return get_value(h, key); }