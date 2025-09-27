// comm/socket_connection.h

#ifndef COMM_SOCKET_CONNECTION_H
#define COMM_SOCKET_CONNECTION_H

#include <stddef.h>
#include <stdint.h>
#include "connection_listener.h"

enum { SOCKETCONN_BUFFER_SIZE = 131068 };

typedef struct SocketConnection {
    int connected;

    // Fake channels so it's possible to build the API without real sockets yet
    unsigned char readBuffer [SOCKETCONN_BUFFER_SIZE];
    unsigned char writeBuffer[SOCKETCONN_BUFFER_SIZE];
    int  readLen;
    int  writeLen;

    uint64_t lastReadMillis;

    ConnectionListener listener;
    void *listener_ctx;

    // Placeholder for future real socket handle
    int placeholder_fd;
} SocketConnection;

// Client-like constructor (no real I/O yet, just marks as connected)
int  SocketConnection_init(SocketConnection *c, const char *ip, int port);
// Server-side accept path analogue (no real I/O yet)
int  SocketConnection_initFromHandle(SocketConnection *c, int fake_handle);
// Clean up and mark disconnected
void SocketConnection_disconnect(SocketConnection *c);
// Returns non-zero if still considered connected
int  SocketConnection_isConnected(const SocketConnection *c);
// Set callbacks
void SocketConnection_setListener(SocketConnection *c,
                                  const ConnectionListener *listener,
                                  void *ctx);
// Write API: returns pointer to internal buffer so caller can memcpy into it
unsigned char *SocketConnection_getWriteBuffer(SocketConnection *c, int *capacity);
// "Tick": pumps write->read locally and calls listener.command() if present
int  SocketConnection_tick(SocketConnection *c);
// Stats analogues
int  SocketConnection_getSentBytes (const SocketConnection *c);
int  SocketConnection_getReadBytes (const SocketConnection *c);
void SocketConnection_clearSentBytes(SocketConnection *c);
void SocketConnection_clearReadBytes(SocketConnection *c);

#endif /* COMM_SOCKET_CONNECTION_H */
