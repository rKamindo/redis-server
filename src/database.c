#include "database.h"
#include "khash.h"
#include "linked_list.h"
#include "util.h"
#include <stdbool.h>

khash_t(redis_hash) * h;

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
      } else if (rv->type == TYPE_LIST) {
        destroy_list(rv->data.list);
      }
      free(rv);
    }
  }
  kh_destroy(redis_hash, h);
}

static void set(khash_t(redis_hash) * h, const char *key, const void *value, ValueType type,
                long long expiration) {
  int ret;
  char *old_key = NULL;

  // check if the key already exists
  khiter_t k = kh_get(redis_hash, h, key);
  if (k != kh_end(h)) {
    old_key = (char *)kh_key(h, k);
  }

  // attempt to put the key into the hash table
  k = kh_put(redis_hash, h, key, &ret);
  if (ret == 0) { // key present, we're updating an existing entry
    RedisValue *old_value = kh_value(h, k);
    if (old_value->type == TYPE_STRING) {
      free(old_value->data.str);
    }
    free(old_key);
    free(old_value);
  }

  kh_key(h, k) = strdup(key);

  RedisValue *redis_value = malloc(sizeof(RedisValue));
  redis_value->type = type;
  redis_value->expiration = expiration;

  // handle the value based on its type
  if (type == TYPE_STRING) {
    redis_value->data.str = strdup(value);
  } else if (type == TYPE_LIST) {
    redis_value->data.list = (List)value;
  }

  kh_value(h, k) = redis_value;
}

static RedisValue *get_string(khash_t(redis_hash) * h, const char *key) {
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

static bool exist(const char *key) {
  khiter_t k = kh_get(redis_hash, h, key);
  return k != kh_end(h) && kh_exist(h, k) ? true : false;
}

static void delete(const char *key) {
  khiter_t k = kh_get(redis_hash, h, key);

  if (k != kh_end(h)) { // check if the key exists
    RedisValue *rv = kh_value(h, k);
    if (rv != NULL) { // additional check to ensure rv is not NULL
      if (rv->type == TYPE_STRING) {
        free(rv->data.str);
      } else if (rv->type == TYPE_LIST) {
        destroy_list(rv->data.list);
      }
      free(rv);
    }
    free((char *)kh_key(h, k));
    kh_del(redis_hash, h, k);
  }
}

void redis_db_create() { create_redis_hash_table(); }
void redis_db_destroy() { destroy_redis_hash(h); }
void redis_db_set(const char *key, const void *value, ValueType type, long long expiration) {
  set(h, key, value, type, expiration);
}
RedisValue *redis_db_get(const char *key) { return get_string(h, key); }
bool redis_db_exist(const char *key) { return exist(key); }
void redis_db_delete(const char *key) { return delete (key); }
int redis_db_lpush(const char *key, const char *item, int *length) {
  RedisValue *existing_value = redis_db_get(key);
  List list;
  if (existing_value != NULL) {
    if (existing_value->type != TYPE_LIST) {
      return ERR_TYPE_MISMATCH;
    }
    list = existing_value->data.list;
  } else {
    // if no existing value, create a new list
    list = create_list();
    redis_db_set(key, list, TYPE_LIST, 0);
  }
  lpush(list, item, length);
  return 0;
}

int redis_db_rpush(const char *key, const char *item, int *length) {
  RedisValue *existing_value = redis_db_get(key);
  List list;
  if (existing_value != NULL) {
    if (existing_value->type != TYPE_LIST) {
      return ERR_TYPE_MISMATCH;
    }
    list = existing_value->data.list;
  } else {
    // if no existing value, create a new list
    list = create_list();
    redis_db_set(key, list, TYPE_LIST, 0);
  }
  rpush(list, item, length);
  return 0;
}

int redis_db_lrange(const char *key, int start, int end, char ***range, int *range_length) {
  RedisValue *existing_value = redis_db_get(key);
  List list;
  if (existing_value != NULL) {
    if (existing_value->type != TYPE_LIST) {
      return ERR_TYPE_MISMATCH;
    }
    list = existing_value->data.list;
  } else {
    // no existing list by the key
    *range_length = 0;
    return ERR_KEY_NOT_FOUND;
  }

  *range = lrange(list, start, end, range_length);
  return 0;
}
