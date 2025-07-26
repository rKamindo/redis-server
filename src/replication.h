#ifndef REPLICATION_H
#define REPLICATION_H

#include "client.h"
#include "database.h"

void master_handle_replica_out(Client *client);
void replica_handle_master_data(Client *master_client);

void set_replica(Client *client);
extern Client *g_replica;
#endif // REPLICATION.H