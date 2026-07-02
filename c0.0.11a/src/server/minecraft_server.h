// server/minecraft_server.h

#ifndef SERVER_MINECRAFT_SERVER_H
#define SERVER_MINECRAFT_SERVER_H

#include <stddef.h>
#include "../comm/socket_server.h"

typedef struct Client Client;

typedef struct MinecraftServer {
    SocketServer socketServer;

    Client *clients[16];
    int count;
} MinecraftServer;

int  MinecraftServer_init(MinecraftServer *s, const unsigned char ips[4], int port);
void MinecraftServer_tick(MinecraftServer *s);
void MinecraftServer_disconnect(MinecraftServer *s, Client *c);

#endif /* SERVER_MINECRAFT_SERVER_H */
