// server.c: MinecraftServer singleton, ported from server/MinecraftServer.java

#include "server.h"
#include "net/connection.h"
#include "net/packet.h"
#include "level/level.h"
#include "level/tile/tile.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#if defined(_WIN32)
  #include <windows.h>
  static void sleepMs(int ms) { Sleep((DWORD)ms); }
#else
  #include <unistd.h>
  static void sleepMs(int ms) { usleep((useconds_t)ms * 1000); }
#endif

static long long nowNanos(void) {
#if defined(_WIN32)
    static LARGE_INTEGER freq = {0};
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (long long)((double)now.QuadPart * 1e9 / (double)freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
#endif
}

static void loadProperties(MinecraftServer* srv) {
    // matches Properties round-tripping: load what's there, default the
    // rest, always re-save so a fresh install gets a populated file
    snprintf(srv->serverName, sizeof srv->serverName, "Minecraft Server");
    snprintf(srv->motd, sizeof srv->motd, "Welcome to my Minecraft Server!");
    srv->port = 25565;
    srv->maxPlayers = 16;
    srv->isPublic = true;

    FILE* f = fopen("server.properties", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof line, f)) {
            if (line[0] == '#') continue;
            char* eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            char* key = line;
            char* value = eq + 1;
            size_t vlen = strlen(value);
            while (vlen > 0 && (value[vlen - 1] == '\n' || value[vlen - 1] == '\r')) value[--vlen] = '\0';

            if (strcmp(key, "server-name") == 0) snprintf(srv->serverName, sizeof srv->serverName, "%s", value);
            else if (strcmp(key, "motd") == 0) snprintf(srv->motd, sizeof srv->motd, "%s", value);
            else if (strcmp(key, "port") == 0) srv->port = atoi(value);
            else if (strcmp(key, "max-players") == 0) srv->maxPlayers = atoi(value);
            else if (strcmp(key, "public") == 0) srv->isPublic = (strcmp(value, "true") == 0);
        }
        fclose(f);
    }

    if (srv->maxPlayers < 1) srv->maxPlayers = 1;
    if (srv->maxPlayers > SERVER_MAX_PLAYERS) srv->maxPlayers = SERVER_MAX_PLAYERS;

    FILE* out = fopen("server.properties", "w");
    if (out) {
        fprintf(out, "#Minecraft server properties\n");
        fprintf(out, "server-name=%s\n", srv->serverName);
        fprintf(out, "motd=%s\n", srv->motd);
        fprintf(out, "port=%d\n", srv->port);
        fprintf(out, "max-players=%d\n", srv->maxPlayers);
        fprintf(out, "public=%s\n", srv->isPublic ? "true" : "false");
        fclose(out);
    }
}

bool Server_init(MinecraftServer* srv) {
    memset(srv, 0, sizeof *srv);
    loadProperties(srv);

    Tile_registerAll();

    if (!Level_load(&srv->level)) {
        Log_info("Generating a new level...");
        Level_init(&srv->level, 256, 256, 64);
    }
    Level_setListener(&srv->level, Server_onBlockChanged, srv);

    PlayerList_init(&srv->admins, "admins.txt");
    PlayerList_init(&srv->bannedNames, "banned.txt");
    PlayerList_init(&srv->bannedIps, "banned-ip.txt");
    PlayerList_init(&srv->onlinePlayers, "players.txt");

    if (!NetSocket_listen(&srv->listenSock, srv->port)) {
        Log_severe("Failed to listen on port %d", srv->port);
        return false;
    }

    Log_info("Now accepting connections on port %d", srv->port);
    return true;
}

int Server_findFreeSlot(const MinecraftServer* srv) {
    for (int i = 0; i < srv->maxPlayers; i++) {
        if (!srv->playerSlots[i]) return i;
    }
    return -1;
}

void Server_broadcastAll(MinecraftServer* srv, const unsigned char* packetBytes, int len) {
    for (int i = 0; i < srv->maxPlayers; i++) {
        Connection* c = srv->playerSlots[i];
        if (c && c->open) Connection_queueOrSend(c, packetBytes, len);
    }
}

void Server_broadcastExcept(MinecraftServer* srv, Connection* exclude, const unsigned char* packetBytes, int len) {
    for (int i = 0; i < srv->maxPlayers; i++) {
        Connection* c = srv->playerSlots[i];
        if (c && c != exclude && c->open) Connection_queueOrSend(c, packetBytes, len);
    }
}

void Server_removeConnection(MinecraftServer* srv, Connection* conn) {
    if (conn->playerId < 0 || srv->playerSlots[conn->playerId] != conn) return;

    if (conn->username[0]) PlayerList_remove(&srv->onlinePlayers, conn->username); // new in server1.3
    Log_info("%s disconnected", conn->username[0] ? conn->username : conn->remoteAddress);
    srv->playerSlots[conn->playerId] = NULL;

    unsigned char pkt[2] = { (unsigned char)PACKET_DESPAWN_PLAYER, (unsigned char)conn->playerId };
    Server_broadcastAll(srv, pkt, 2);

    if (conn->spawned) {
        char msg[80];
        snprintf(msg, sizeof msg, "%s left the game", conn->username);
        unsigned char msgPkt[1 + 1 + PACKET_STRING_LEN];
        int n = 0;
        msgPkt[n++] = (unsigned char)PACKET_MESSAGE;
        msgPkt[n++] = (unsigned char)0xFF;
        size_t mlen = strlen(msg);
        if (mlen > PACKET_STRING_LEN) mlen = PACKET_STRING_LEN;
        for (size_t i = 0; i < (size_t)PACKET_STRING_LEN; i++) msgPkt[n++] = (unsigned char)(i < mlen ? msg[i] : ' ');
        Server_broadcastAll(srv, msgPkt, n);
    }

    conn->playerId = -1;
}

static bool equalsIgnoreCase(const char* a, const char* b) {
    char aa[65], bb[65];
    snprintf(aa, sizeof aa, "%s", a);
    snprintf(bb, sizeof bb, "%s", b);
    for (char* p = aa; *p; p++) *p = (char)tolower((unsigned char)*p);
    for (char* p = bb; *p; p++) *p = (char)tolower((unsigned char)*p);
    return strcmp(aa, bb) == 0;
}

static Connection* findOnlineByName(MinecraftServer* srv, const char* name) {
    for (int i = 0; i < srv->maxPlayers; i++) {
        Connection* c = srv->playerSlots[i];
        if (c && c->loggedIn) {
            char a[65], b[65];
            snprintf(a, sizeof a, "%s", c->username);
            snprintf(b, sizeof b, "%s", name);
            for (char* p = a; *p; p++) *p = (char)tolower((unsigned char)*p);
            for (char* p = b; *p; p++) *p = (char)tolower((unsigned char)*p);
            if (strcmp(a, b) == 0) return c;
        }
    }
    return NULL;
}

void Server_kickByName(MinecraftServer* srv, const char* name) {
    Connection* c = findOnlineByName(srv, name);
    if (c) Connection_kick(c, "You were kicked");
}

void Server_kickExistingSessionByName(MinecraftServer* srv, const char* name) {
    Connection* c = findOnlineByName(srv, name);
    if (c) Connection_kick(c, "You logged in from another computer.");
}

void Server_banByName(MinecraftServer* srv, const char* name) {
    PlayerList_add(&srv->bannedNames, name);
    Connection* c = findOnlineByName(srv, name);
    if (c) Connection_kick(c, "You were banned");
}

void Server_unbanByName(MinecraftServer* srv, const char* name) {
    PlayerList_remove(&srv->bannedNames, name);
}

void Server_opByName(MinecraftServer* srv, const char* name) {
    PlayerList_add(&srv->admins, name);
    Connection* c = findOnlineByName(srv, name);
    if (c) Connection_sendSystemMessage(c, "You're now op!");
}

void Server_deopByName(MinecraftServer* srv, const char* name) {
    PlayerList_remove(&srv->admins, name);
    Connection* c = findOnlineByName(srv, name);
    if (c) Connection_sendSystemMessage(c, "You're no longer op!");
}

void Server_teleportToPlayer(MinecraftServer* srv, Connection* issuer, const char* targetName) {
    Connection* target = findOnlineByName(srv, targetName);
    if (!target) return;

    unsigned char pkt[9];
    int n = 0;
    pkt[n++] = (unsigned char)PACKET_TELEPORT;
    pkt[n++] = (unsigned char)0xFF; // -1: self, same sentinel as the initial spawn packet
    pkt[n++] = (unsigned char)(target->lastX >> 8); pkt[n++] = (unsigned char)target->lastX;
    pkt[n++] = (unsigned char)(target->lastY >> 8); pkt[n++] = (unsigned char)target->lastY;
    pkt[n++] = (unsigned char)(target->lastZ >> 8); pkt[n++] = (unsigned char)target->lastZ;
    pkt[n++] = (unsigned char)target->lastYaw;
    pkt[n++] = (unsigned char)target->lastPitch;
    Connection_queueOrSend(issuer, pkt, n);
}

void Server_banIpByName(MinecraftServer* srv, const char* name) {
    // new in server1.3: matches by username OR by raw IP (with or without a
    // leading slash, an artifact of Java's InetAddress.toString() that this
    // port's remoteAddress never has to begin with), against every connected
    // session, not just the first match by name
    const char* ipMatch = name;
    if (name[0] == '/') ipMatch = name + 1;

    bool any = false;
    char msg[256];
    msg[0] = '\0';
    for (int i = 0; i < srv->maxPlayers; i++) {
        Connection* c = srv->playerSlots[i];
        if (!c || !c->loggedIn) continue;
        if (!equalsIgnoreCase(c->username, name) && !equalsIgnoreCase(c->remoteAddress, ipMatch)) continue;

        PlayerList_add(&srv->bannedIps, c->remoteAddress);
        Connection_kick(c, "You were banned");

        // replicates the real source's own message-building bug exactly
        // (str == "" is a reference equality check that only ever holds for
        // the literal first empty string, so only the very first match gets
        // a leading ", " and no match ever gets a separator from the next
        // one) rather than silently producing well formed comma separated
        // output
        if (msg[0] == '\0') snprintf(msg, sizeof msg, ", ");
        snprintf(msg + strlen(msg), sizeof msg - strlen(msg), "%s", c->username);
        any = true;
    }

    if (any) {
        char broadcast[256];
        snprintf(broadcast, sizeof broadcast, "%s got ip banned!", msg);
        unsigned char pkt[1 + 1 + PACKET_STRING_LEN];
        int n = 0;
        pkt[n++] = (unsigned char)PACKET_MESSAGE;
        pkt[n++] = (unsigned char)0xFF;
        size_t len = strlen(broadcast);
        if (len > PACKET_STRING_LEN) len = PACKET_STRING_LEN;
        for (size_t i = 0; i < (size_t)PACKET_STRING_LEN; i++) pkt[n++] = (unsigned char)(i < len ? broadcast[i] : ' ');
        Server_broadcastAll(srv, pkt, n);
    }
}

void Server_onBlockChanged(void* ctx, int x, int y, int z) {
    MinecraftServer* srv = (MinecraftServer*)ctx;
    int type = Level_getTile(&srv->level, x, y, z);
    unsigned char pkt[8] = {
        (unsigned char)PACKET_SET_BLOCK_SC,
        (unsigned char)(x >> 8), (unsigned char)x,
        (unsigned char)(y >> 8), (unsigned char)y,
        (unsigned char)(z >> 8), (unsigned char)z,
        (unsigned char)type
    };
    Server_broadcastAll(srv, pkt, 8);
}

void Server_run(MinecraftServer* srv) {
    // matches run(): network I/O every outer iteration, a fixed ~20Hz world
    // tick, a slower ~0.5s ping broadcast, autosave every ~60s. Heartbeat to
    // the long dead minecraft.net master list is deliberately not ported
    const long long tickNanos = 50000000LL;  // 50ms = 20Hz
    const long long pingNanos = 500000000LL; // 0.5s

    long long lastTick = nowNanos();
    long long tickAccum = 0;
    long long pingAccum = 0;

    // static, not stack local: each Connection carries 3 x 256KB buffers,
    // so the full array is tens of megabytes, comfortably past a typical
    // thread's default stack size
    static Connection connections[SERVER_MAX_PLAYERS];
    static bool connectionUsed[SERVER_MAX_PLAYERS];

    for (;;) {
        // accept new connections
        sock_t clientSock;
        while (NetSocket_accept(srv->listenSock, &clientSock)) {
            char addr[64];
            NetSocket_getRemoteAddress(clientSock, addr, sizeof addr);

            if (PlayerList_contains(&srv->bannedIps, addr)) {
                NetSocket_configure(clientSock);
                NetSocket_close(clientSock);
                continue;
            }

            // new in server1.3: reject a 4th simultaneous connection from the
            // same address, checked by IP at accept time before any login,
            // matching the real source exactly (this is IP based, not
            // username based, despite how it reads at first glance)
            int sameAddrCount = 0;
            for (int i = 0; i < SERVER_MAX_PLAYERS; i++) {
                if (connectionUsed[i] && strcmp(connections[i].remoteAddress, addr) == 0) sameAddrCount++;
            }
            if (sameAddrCount >= 3) {
                NetSocket_configure(clientSock);
                NetSocket_close(clientSock);
                continue;
            }

            int slot = Server_findFreeSlot(srv);
            int freeConnIdx = -1;
            for (int i = 0; i < SERVER_MAX_PLAYERS; i++) if (!connectionUsed[i]) { freeConnIdx = i; break; }

            if (slot < 0 || freeConnIdx < 0) {
                NetSocket_configure(clientSock);
                NetSocket_close(clientSock);
                continue;
            }

            Connection* c = &connections[freeConnIdx];
            connectionUsed[freeConnIdx] = true;
            Connection_init(c, srv, clientSock);
            c->playerId = slot;
            srv->playerSlots[slot] = c;
        }

        // network I/O for every connection
        for (int i = 0; i < SERVER_MAX_PLAYERS; i++) {
            if (!connectionUsed[i]) continue;
            Connection_tick(&connections[i]);
            if (!connections[i].open) {
                Server_removeConnection(srv, &connections[i]);
                connectionUsed[i] = false;
            }
        }

        long long now = nowNanos();
        long long elapsed = now - lastTick;
        lastTick = now;
        if (elapsed < 0) elapsed = 0;
        if (elapsed > 1000000000LL) elapsed = 1000000000LL;

        tickAccum += elapsed;
        while (tickAccum >= tickNanos) {
            tickAccum -= tickNanos;
            srv->tickCount++;
            Level_onTick(&srv->level);

            if (srv->tickCount - srv->lastSaveTick >= 1200) {
                srv->lastSaveTick = srv->tickCount;
                Log_info("Saving level");
                Level_save(&srv->level);
            }
        }

        pingAccum += elapsed;
        if (pingAccum >= pingNanos) {
            pingAccum = 0;
            unsigned char pingPkt[1] = { (unsigned char)PACKET_PING };
            Server_broadcastAll(srv, pingPkt, 1);
        }

        sleepMs(5);
    }
}
