#ifndef SERVER_H
#define SERVER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "client.h"
#include "command_handler.h"
#include "khash.h"
#include "resp.h"

// foward declarations
struct CommandHandler;

// type definitions
typedef enum { TYPE_STRING } ValueType;

typedef struct {
  ValueType type;
  union {
    char *str;
  } data;
} RedisValue;

KHASH_MAP_INIT_STR(redis_hash, RedisValue *)

void set_value(khash_t(redis_hash) * h, const char *key, const void *value, ValueType type);
RedisValue *get_value(khash_t(redis_hash) * h, const char *key);
void cleanup_hash(khash_t(redis_hash) * h);
void handle_command(struct CommandHandler *ch);
int start_server();

#ifdef __cplusplus
}
#endif

#endif // SERVER_H