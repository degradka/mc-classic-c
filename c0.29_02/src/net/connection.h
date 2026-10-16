// net/connection.h: the connection to a server

#ifndef NET_CONNECTION_H
#define NET_CONNECTION_H

#include <stdbool.h>
#include "net_socket.h"

#define NET_READ_BUFFER_SIZE  (256 * 1024)
#define NET_WRITE_BUFFER_SIZE (32 * 1024)

typedef struct {
    sock_t sock;
    bool   connected;

    // 0 still connecting, 1 succeeded, 2 failed
    // the connect thread owns sock, host, port and username until then
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

    // set once the level is downloaded, the self Teleport waits for it
    bool levelLoaded;
} NetConnection;

// connects on its own thread, poll NetConnection_pollConnecting every tick
// the connect thread, started before the window opens
void NetConnection_startConnect(NetConnection* c, const char* host, int port, const char* username);
// the "Connecting.." screen while it runs
void NetConnection_beginConnect(NetConnection* c);

// true once the connect finished either way
// the Login or the error screen has been handled by then
bool NetConnection_pollConnecting(NetConnection* c);

void NetConnection_close(NetConnection* c);

// up to 100 packets a tick, then the writes are flushed
// an error closes the connection and shows the disconnect screen
void NetConnection_tick(NetConnection* c);

void NetConnection_sendSetBlock(NetConnection* c, int x, int y, int z, int mode, int type);
void NetConnection_sendMessage(NetConnection* c, const char* text);
// sent every tick with id -1, no dirty check
void NetConnection_sendTeleportSelf(NetConnection* c, float x, float y, float z, float yaw, float pitch);

#endif
