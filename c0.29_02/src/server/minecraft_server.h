// server/minecraft_server.h

#ifndef SERVER_MINECRAFT_SERVER_H
#define SERVER_MINECRAFT_SERVER_H

#include "../comm/socket_server.h"

typedef struct Client Client;

// a linear scan, there are never many clients
typedef struct MinecraftServer {
    SocketServer socketServer;
    Client* clients[SOCKET_SERVER_MAX_CONNECTIONS];
    int count;
} MinecraftServer;

int  MinecraftServer_init(MinecraftServer* s, const unsigned char ips[4], int port);
void MinecraftServer_tick(MinecraftServer* s);
void MinecraftServer_disconnect(MinecraftServer* s, Client* c);

// ticks forever with a 5ms sleep, the game never calls it
void MinecraftServer_run(MinecraftServer* s);

#endif /* SERVER_MINECRAFT_SERVER_H */
