// server.h: MinecraftServer

#ifndef SERVER_H
#define SERVER_H

#include "jstring.h"
#include "player.h"
#include "player_list.h"
#include "properties.h"
#include "level/level.h"
#include "net/connection.h"
#include <stdbool.h>

// a kicked or refused connection, flushed for 40 ticks and then closed
typedef struct {
    SocketConnection* conn;
    int ticks;
} PendingClose;

typedef struct MinecraftServer {
    sock_t listenSock;
    // every open socket, the joined players and the ones waiting to close, in list order
    SocketConnection** conns;
    int connCount, connCap;
    Player** players;
    int playerCount, playerCap;
    PendingClose* closing;
    int closingCount, closingCap;
    // gone players whose level thread hasn't finished
    Player** dead;
    int deadCount, deadCap;

    Player* slots[32];
    int maxPlayers;
    int port;
    bool isPublic;
    JStr serverName;
    JStr motd;
    Properties properties;
    Level* level;
    PlayerList admins;
    PlayerList banned;
    PlayerList bannedIp;
    // who's online, by name
    PlayerList playerList;
    bool verifyNames;
    // copied to the level, it only stops dandelions and roses ticking
    bool growTrees;
    // what the name check hashes with, minecraft.net got it with the heartbeat
    JStr salt;
    int maxConnections;
} MinecraftServer;

// false with the exception's toString when the port can't be opened
bool Server_init(MinecraftServer* s, char* exception, size_t size);
void Server_run(MinecraftServer* s);
void Server_broadcast(MinecraftServer* s, int id, const PacketValue* values);
void Server_broadcastExcept(MinecraftServer* s, const Player* except, int id, const PacketValue* values);
void Server_closeLater(MinecraftServer* s, SocketConnection* conn);
void Server_tileChanged(void* ctx, int x, int y, int z);
void Server_kick(MinecraftServer* s, const JStr* name);
void Server_ban(MinecraftServer* s, const JStr* name);
void Server_op(MinecraftServer* s, const JStr* name);
void Server_deop(MinecraftServer* s, const JStr* name);
void Server_unban(MinecraftServer* s, const JStr* name);
void Server_banIp(MinecraftServer* s, const JStr* name);
void Server_save(MinecraftServer* s, bool quiet);
Player* Server_findPlayer(MinecraftServer* s, const JStr* name);
// an admin command without its slash, from a player or the console (NULL)
void Server_command(MinecraftServer* s, Player* p, const JStr* command);
// md5 of the salt and the name in hex, what a client from minecraft.net sends as its key
void Server_nameKey(const MinecraftServer* s, const JStr* name, JStr* out);

#endif
