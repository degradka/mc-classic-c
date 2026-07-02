// comm/socket_server.h

#ifndef COMM_SOCKET_SERVER_H
#define COMM_SOCKET_SERVER_H

#include "server_listener.h"
#include "socket_connection.h"

typedef struct SocketServer {
    ServerListener listener;
    SocketConnection conns[8];
    int count;
    unsigned char ips[4];
    int port;
} SocketServer;

int  SocketServer_init(SocketServer *s, const unsigned char ips[4], int port,
                       const ServerListener *listener);
void SocketServer_tick(SocketServer *s);

#endif /* COMM_SOCKET_SERVER_H */
