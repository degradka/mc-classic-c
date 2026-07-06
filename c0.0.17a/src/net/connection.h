// net/connection.h: the connection to a server

#ifndef NET_CONNECTION_H
#define NET_CONNECTION_H

#include <stdbool.h>
#include <stdint.h>

#define NET_BUFFER_SIZE (1024 * 1024)

// what the read does to the caller, NET_THROW is an exception out of it
typedef enum { NET_OK, NET_THROW } NetResult;

typedef struct {
    // a sock_t, kept as an integer so winsock stays out of everything that includes this
    uintptr_t sock;
    // the connect thread hasn't finished yet
    bool   pending;
    char   host[256];
    int    port;
    // the socket exists and is open
    bool   hasSocket;
    bool   connected;
    // the login went out, packets get handled from now on
    bool   loggedIn;
    // a close that couldn't flush everything, the next write throws
    bool   unflushed;
    char   username[65];
    char   mppass[65];

    unsigned char readBuf[NET_BUFFER_SIZE];
    int readLen;

    unsigned char writeBuf[NET_BUFFER_SIZE];
    int writeLen;

    // the gzipped level, gathered from the chunk packets
    unsigned char* levelBuf;
    int levelBufLen;
    int levelBufCapacity;
} NetConnection;

// the connect waits for NetConnection_finishConnect
void NetConnection_open(NetConnection* c, const char* host, int port, const char* username, const char* mppass);
// a blocking connect at the end of a tick
// 1 connected and the login is queued, 0 nothing yet, -1 the connect failed
// a name that doesn't resolve kills the connect thread, so it stays connecting forever
int  NetConnection_finishConnect(NetConnection* c);
bool NetConnection_isOpen(const NetConnection* c);
// flushes what's queued and closes
void NetConnection_close(NetConnection* c);

// reads, handles up to 100 packets and flushes the writes
NetResult NetConnection_tick(NetConnection* c);

void NetConnection_sendSetBlock(NetConnection* c, int x, int y, int z, int mode, int type);
void NetConnection_sendMessage(NetConnection* c, const char* text);
void NetConnection_sendPosition(NetConnection* c, float x, float y, float z, float yRot, float xRot);

#endif
