#ifndef REPLICATION_H
#define REPLICATION_H

#include "client.h"
#include "database.h"

void master_handle_replica_out(Client *client);
void replica_handle_master_data(Client *master_client);

void add_replica(Client *replica);
void remove_replica(Client *replica);

#endif // REPLICATION.H