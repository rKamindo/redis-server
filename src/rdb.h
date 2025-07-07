#ifndef RDB_H
#define RDB_H

#include "database.h"

int rdb_load_data_from_file(redis_db_t *db, const char *dir, const char *filename);
bool rdb_save_data_to_file(redis_db_t *db, const char *dir, const char *filename);

#endif // RDB_H
