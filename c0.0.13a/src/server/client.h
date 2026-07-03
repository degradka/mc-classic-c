// server/client.h

#ifndef SERVER_CLIENT_H
#define SERVER_CLIENT_H

#include "../comm/socket_connection.h"
#include "../comm/connection_listener.h"

struct MinecraftServer;

// port of server.Client: a per connection ConnectionListener that currently
// does nothing with incoming data, matching the empty command() in Java
typedef struct Client {
    struct MinecraftServer* server;
    SocketConnection* conn;
    ConnectionListener listener;
} Client;

void Client_init(Client* c, struct MinecraftServer* server, SocketConnection* conn);
void Client_disconnect(Client* c);

#endif /* SERVER_CLIENT_H */
