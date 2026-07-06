// net/connection.h: per-connection session, ported from
// com.mojang.minecraft.net.c + com.mojang.a.a combined (the real source
// splits socket plumbing from session state across those two classes, kept
// together here since nothing else uses the raw socket independently)

#ifndef NET_CONNECTION_H
#define NET_CONNECTION_H

#include <stdbool.h>
#include "net_socket.h"

#define NET_READ_BUFFER_SIZE  (256 * 1024)
#define NET_WRITE_BUFFER_SIZE (32 * 1024)

typedef struct {
    sock_t sock;
    bool   connected;

    unsigned char readBuf[NET_READ_BUFFER_SIZE];
    int readLen;

    unsigned char writeBuf[NET_WRITE_BUFFER_SIZE];
    int writeLen;

    // accumulates the gzipped level stream across LevelChunk packets
    unsigned char* levelBuf;
    int levelBufLen;
    int levelBufCapacity;
} NetConnection;

// blocking connect, then sends the Login packet and opens the loading
// screen ("Connecting.."), matching net.c's constructor. Returns false on
// a connect failure, having already shown the connect-failure message screen
bool NetConnection_open(NetConnection* c, const char* host, int port, const char* username);
void NetConnection_close(NetConnection* c);

// drains up to 100 queued packets and dispatches each, then flushes any
// pending writes. Matches com.mojang.a.a.b(), called once per game tick.
// Closes the connection and opens the disconnect message screen on error
void NetConnection_tick(NetConnection* c);

void NetConnection_sendSetBlock(NetConnection* c, int x, int y, int z, int mode, int type);
void NetConnection_sendMessage(NetConnection* c, const char* text);
// sent unconditionally every tick with id -1, no dirty check, matching the
// real source's per-tick self broadcast
void NetConnection_sendTeleportSelf(NetConnection* c, float x, float y, float z, float yaw, float pitch);

#endif
