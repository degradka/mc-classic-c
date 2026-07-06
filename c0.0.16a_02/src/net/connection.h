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

    unsigned char readBuf[NET_READ_BUFFER_SIZE];
    int readLen;

    unsigned char writeBuf[NET_WRITE_BUFFER_SIZE];
    int writeLen;

    // the gzipped level, gathered from the chunk packets
    unsigned char* levelBuf;
    int levelBufLen;
    int levelBufCapacity;
} NetConnection;

// connects, sends Login and shows "Connecting.."
// false on failure, the error screen is already up
bool NetConnection_open(NetConnection* c, const char* host, int port, const char* username);
void NetConnection_close(NetConnection* c);

// up to 100 packets a tick, then the writes are flushed
// an error closes the connection and shows the disconnect screen
void NetConnection_tick(NetConnection* c);

void NetConnection_sendSetBlock(NetConnection* c, int x, int y, int z, int mode, int type);
void NetConnection_sendMessage(NetConnection* c, const char* text);
void NetConnection_sendTeleportSelf(NetConnection* c, float x, float y, float z, float yaw, float pitch);

#endif
