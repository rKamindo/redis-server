#ifdef __cplusplus
extern "C" {
#endif

#include "khash.h"

// type definitions
typedef enum { TYPE_STRING } ValueType;

typedef struct {
  ValueType type;
  union {
    char *str;
  } data;
} RedisValue;

KHASH_MAP_INIT_STR(redis_hash, RedisValue);

void set_value(khash_t(redis_hash) * h, const char *key, const void *value,
               ValueType type);
RedisValue *get_value(khash_t(redis_hash) * h, const char *key);
void cleanup_hash(khash_t(redis_hash) * h);
void send_response(int ConnectFD, const char *respone);
int start_server();

#ifdef __cplusplus
}
#endif