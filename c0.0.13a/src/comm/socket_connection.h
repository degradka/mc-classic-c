// comm/socket_connection.h

#ifndef COMM_SOCKET_CONNECTION_H
#define COMM_SOCKET_CONNECTION_H

#include <stddef.h>
#include <stdint.h>
#include "connection_listener.h"

enum { SOCKETCONN_BUFFER_SIZE = 131068 };

typedef struct SocketConnection {
    int connected;

    // no real sockets yet, buffers just loop write into read locally
    unsigned char readBuffer [SOCKETCONN_BUFFER_SIZE];
    unsigned char writeBuffer[SOCKETCONN_BUFFER_SIZE];
    int  readLen;
    int  writeLen;

    uint64_t lastReadMillis;

    ConnectionListener listener;
    void *listener_ctx;

    int placeholder_fd;
} SocketConnection;

int  SocketConnection_init(SocketConnection *c, const char *ip, int port);
int  SocketConnection_initFromHandle(SocketConnection *c, int fake_handle);
void SocketConnection_disconnect(SocketConnection *c);
int  SocketConnection_isConnected(const SocketConnection *c);
void SocketConnection_setListener(SocketConnection *c,
                                  const ConnectionListener *listener,
                                  void *ctx);
unsigned char *SocketConnection_getWriteBuffer(SocketConnection *c, int *capacity);
int  SocketConnection_tick(SocketConnection *c);
int  SocketConnection_getSentBytes (const SocketConnection *c);
int  SocketConnection_getReadBytes (const SocketConnection *c);
void SocketConnection_clearSentBytes(SocketConnection *c);
void SocketConnection_clearReadBytes(SocketConnection *c);

#endif /* COMM_SOCKET_CONNECTION_H */
