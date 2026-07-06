// server.h: the MinecraftServer singleton, ported from server/MinecraftServer.java

#ifndef SERVER_H
#define SERVER_H

#include "level/level.h"
#include "player_list.h"
#include "net/net_socket.h"
#include <stdbool.h>

// only ever used as a pointer here (an array of them), kept opaque to avoid
// a circular full include with connection.h (whose Connection holds a
// MinecraftServer* backref)
struct Connection;

#define SERVER_MAX_PLAYERS 32

typedef struct MinecraftServer {
    sock_t listenSock;
    Level level;

    // fixed slot array, index doubles as the wire protocol's player id.
    // NULL = free slot, matches PlayerConnection[] playerSlots
    struct Connection* playerSlots[SERVER_MAX_PLAYERS];
    int maxPlayers;

    char serverName[65];
    char motd[65];
    int port;
    bool isPublic;

    PlayerList admins;
    PlayerList bannedNames;
    PlayerList bannedIps;
    // new in server1.3: a live "who is online right now" roster, added on
    // login and removed on disconnect, not related to permissions
    PlayerList onlinePlayers;

    long long tickCount;
    long long lastSaveTick;
    long long lastHeartbeatTick;
} MinecraftServer;

bool Server_init(MinecraftServer* srv);
void Server_run(MinecraftServer* srv); // never returns, matches MinecraftServer.run()

// -1 if full, matching findFreeSlot()
int Server_findFreeSlot(const MinecraftServer* srv);

void Server_broadcastAll(MinecraftServer* srv, const unsigned char* packetBytes, int len);
void Server_broadcastExcept(MinecraftServer* srv, struct Connection* exclude, const unsigned char* packetBytes, int len);

// removes a connection from its slot and broadcasts DespawnPlayer, matching
// MinecraftServer.removeConnection(). No-op if it's already been removed
void Server_removeConnection(MinecraftServer* srv, struct Connection* conn);

// matches kickByName/banByName/opByName/deopByName/unbanByName/banIpByName:
// act on the persisted list regardless of online status, and additionally
// kick/notify the matching online connection if there is one
void Server_kickByName(MinecraftServer* srv, const char* name);
// new in server1.3: kicks whatever connection is currently logged in as name,
// if any, with the real source's exact relogin message. Called from
// handleLogin before the new connection's own checks, matching real ordering
void Server_kickExistingSessionByName(MinecraftServer* srv, const char* name);
void Server_banByName(MinecraftServer* srv, const char* name);
void Server_unbanByName(MinecraftServer* srv, const char* name);
void Server_opByName(MinecraftServer* srv, const char* name);
void Server_deopByName(MinecraftServer* srv, const char* name);
void Server_banIpByName(MinecraftServer* srv, const char* name);

// matches MinecraftServer.a(x,y,z): the Level block change listener callback,
// reads the current tile back and broadcasts SetBlock server->client to everyone
void Server_onBlockChanged(void* ctx, int x, int y, int z);

#endif
