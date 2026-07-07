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

    // new in c0.0.17a: connecting happens on a background thread instead of
    // blocking the render thread, matching net/c.java becoming a Thread
    // subclass. 0 = still connecting, 1 = succeeded, 2 = failed. sock/host/
    // port/username are only touched by the background thread until it
    // publishes a result, then only by the main thread
    volatile int connectResult;
    sock_t connectingSock;
    char connectHost[256];
    int connectPort;
    char connectUsername[65];

    unsigned char readBuf[NET_READ_BUFFER_SIZE];
    int readLen;

    unsigned char writeBuf[NET_WRITE_BUFFER_SIZE];
    int writeLen;

    // accumulates the gzipped level stream across LevelChunk packets
    unsigned char* levelBuf;
    int levelBufLen;
    int levelBufCapacity;

    // c0.0.19a_04: set once the level finishes downloading and installing.
    // The per-tick self Teleport send is now gated on this instead of firing
    // unconditionally from the moment the socket connects, so the server no
    // longer sees movement data arrive while the client is still mid-download
    bool levelLoaded;
} NetConnection;

// kicks off the connect on a background thread and returns immediately,
// matching net/c.java's constructor becoming an async Thread in c0.0.17a.
// Call NetConnection_pollConnecting every tick afterward until it returns
// true
void NetConnection_beginConnect(NetConnection* c, const char* host, int port, const char* username);

// returns true once the background connect attempt has finished (either way).
// On success, connected is now true, the Login packet has been sent, and the
// "Connecting.." loading screen has already been started. On failure,
// connected stays false and the connect-failure message screen has already
// been shown. Returns false, with nothing else changed, while still pending
bool NetConnection_pollConnecting(NetConnection* c);

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
