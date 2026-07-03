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

// port of comm.SocketConnection: a non blocking socket with its own read
// and write byte buffers, ticked once per server/client loop iteration
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

// client side, matches the SocketConnection(String ip, int port) constructor
int  SocketConnection_open(SocketConnection* c, const char* ip, int port);
// server side, matches the SocketConnection(SocketChannel) constructor
void SocketConnection_fromAccepted(SocketConnection* c, sock_t sock);

void SocketConnection_setListener(SocketConnection* c, ConnectionListener listener, void* ctx);
int  SocketConnection_isConnected(const SocketConnection* c);
void SocketConnection_disconnect(SocketConnection* c);

// matches getBuffer(): a direct view into the write buffer to append to
unsigned char* SocketConnection_getBuffer(SocketConnection* c, int* capacity);
void SocketConnection_advanceWrite(SocketConnection* c, int n);

// flushes pending writes, reads what is available, and calls the listener's
// command callback once if any bytes are buffered, matching tick()
void SocketConnection_tick(SocketConnection* c);

int  SocketConnection_getSentBytes(const SocketConnection* c);
int  SocketConnection_getReadBytes(const SocketConnection* c);
void SocketConnection_clearSentBytes(SocketConnection* c);
void SocketConnection_clearReadBytes(SocketConnection* c);

#endif /* COMM_SOCKET_CONNECTION_H */
