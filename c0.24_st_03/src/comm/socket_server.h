// comm/socket_server.h

#ifndef COMM_SOCKET_SERVER_H
#define COMM_SOCKET_SERVER_H

#include "server_listener.h"
#include "socket_connection.h"

enum { SOCKET_SERVER_MAX_CONNECTIONS = 64 };

// non blocking listen socket, accepts clients and ticks every connection
typedef struct SocketServer {
    sock_t listenSock;
    ServerListener listener;
    SocketConnection* connections[SOCKET_SERVER_MAX_CONNECTIONS];
    int count;
} SocketServer;

int  SocketServer_init(SocketServer* s, const unsigned char ip[4], int port, ServerListener listener);
void SocketServer_tick(SocketServer* s);

#endif /* COMM_SOCKET_SERVER_H */
