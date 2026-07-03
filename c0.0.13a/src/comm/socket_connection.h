// comm/socket_connection.h

#ifndef COMM_SOCKET_CONNECTION_H
#define COMM_SOCKET_CONNECTION_H

#include "connection_listener.h"

#if defined(_WIN32)
  #include <winsock2.h>
  typedef SOCKET sock_t;
#else
  typedef int sock_t;
#endif

enum { SOCKETCONN_BUFFER_SIZE = 131068 };

// non blocking socket with its own read and write buffers
typedef struct SocketConnection {
    sock_t sock;
    int connected;

    unsigned char readBuffer [SOCKETCONN_BUFFER_SIZE];
    unsigned char writeBuffer[SOCKETCONN_BUFFER_SIZE];
    int readLen;
    int writeLen;

    long long lastRead;

    ConnectionListener listener;
    void* listenerCtx;

    int bytesRead;
    int totalBytesWritten;
} SocketConnection;

// client side
int  SocketConnection_open(SocketConnection* c, const char* ip, int port);
// server side
void SocketConnection_fromAccepted(SocketConnection* c, sock_t sock);

void SocketConnection_setListener(SocketConnection* c, ConnectionListener listener, void* ctx);
int  SocketConnection_isConnected(const SocketConnection* c);
void SocketConnection_disconnect(SocketConnection* c);

// the write buffer to append to
unsigned char* SocketConnection_getBuffer(SocketConnection* c, int* capacity);
void SocketConnection_advanceWrite(SocketConnection* c, int n);

// flushes writes, reads what's there
// and calls the listener once if anything is buffered
void SocketConnection_tick(SocketConnection* c);

int  SocketConnection_getSentBytes(const SocketConnection* c);
int  SocketConnection_getReadBytes(const SocketConnection* c);
void SocketConnection_clearSentBytes(SocketConnection* c);
void SocketConnection_clearReadBytes(SocketConnection* c);

#endif /* COMM_SOCKET_CONNECTION_H */
