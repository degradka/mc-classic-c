// net/connection.h: the connection to a server

#ifndef NET_CONNECTION_H
#define NET_CONNECTION_H

#include <stdbool.h>
#include <stdint.h>

#define NET_BUFFER_SIZE (1024 * 1024)

typedef struct {
    // a sock_t, kept as an integer so winsock stays out of everything that includes this
    uintptr_t sock;
    bool   connected;

    unsigned char readBuf[NET_BUFFER_SIZE];
    int readLen;

    unsigned char writeBuf[NET_BUFFER_SIZE];
    int writeLen;

    // the gzipped level, gathered from the chunk packets
    unsigned char* levelBuf;
    int levelBufLen;
    int levelBufCapacity;
} NetConnection;

// connects and queues the login, false if the connect failed
bool NetConnection_open(NetConnection* c, const char* host, int port, const char* username);
// flushes what's queued and closes
void NetConnection_close(NetConnection* c);

// reads, handles up to 100 packets and flushes the writes
// false means it threw, the caller shows the disconnect screen and closes
bool NetConnection_tick(NetConnection* c);

void NetConnection_sendSetBlock(NetConnection* c, int x, int y, int z, int mode, int type);
void NetConnection_sendMessage(NetConnection* c, const char* text);
void NetConnection_sendPosition(NetConnection* c, float x, float y, float z, float yRot, float xRot);

#endif
