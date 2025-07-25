#ifndef REPLICATION_H
#define REPLICATION_H

#include "client.h"
#include "database.h"
#include "server_config.h"

void master_handle_replica_out(Client *client);
void replica_handle_master_data(Client *master_client);

#endif // REPLICATION.H