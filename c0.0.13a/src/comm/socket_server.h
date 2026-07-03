// comm/socket_server.h

#ifndef COMM_SOCKET_SERVER_H
#define COMM_SOCKET_SERVER_H

#include "server_listener.h"
#include "socket_connection.h"

enum { SOCKET_SERVER_MAX_CONNECTIONS = 64 };

// port of comm.SocketServer: a non blocking listening socket that accepts
// clients and ticks each open SocketConnection once per call
typedef struct SocketServer {
    sock_t listenSock;
    ServerListener listener;
    // owned pointers, matching Java's List<SocketConnection> of references
    SocketConnection* connections[SOCKET_SERVER_MAX_CONNECTIONS];
    int count;
} SocketServer;

int  SocketServer_init(SocketServer* s, const unsigned char ip[4], int port, ServerListener listener);
void SocketServer_tick(SocketServer* s);

#endif /* COMM_SOCKET_SERVER_H */
