#ifndef RDB_H
#define RDB_H

#include "client.h"
#include "database.h"

int rdb_load_data_from_file(redis_db_t *db, const char *dir, const char *filename);
bool rdb_save_data_to_file(redis_db_t *db, const char *dir, const char *filename);
void master_send_rdb_snapshot(Client *client);
void replica_receive_rdb_snapshot(Client *client);

#endif // RDB_H
