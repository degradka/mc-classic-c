// comm/socket_server.c

#include "socket_server.h"
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32)
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #define INVALID_LISTEN_SOCK INVALID_SOCKET
  #define CLOSESOCK(s) closesocket(s)
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  #include <fcntl.h>
  #define INVALID_LISTEN_SOCK (-1)
  #define CLOSESOCK(s) close(s)
#endif

static void setNonBlocking(sock_t s) {
#if defined(_WIN32)
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}

int SocketServer_init(SocketServer* s, const unsigned char ip[4], int port, ServerListener listener) {
    memset(s, 0, sizeof(*s));
    s->listener = listener;

#if defined(_WIN32)
    static int inited = 0;
    if (!inited) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        inited = 1;
    }
#endif

    s->listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s->listenSock == INVALID_LISTEN_SOCK) return 0;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    memcpy(&addr.sin_addr, ip, 4);

    if (bind(s->listenSock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        CLOSESOCK(s->listenSock);
        return 0;
    }
    if (listen(s->listenSock, 8) != 0) {
        CLOSESOCK(s->listenSock);
        return 0;
    }
    setNonBlocking(s->listenSock);
    return 1;
}

void SocketServer_tick(SocketServer* s) {
    for (;;) {
        sock_t accepted = accept(s->listenSock, NULL, NULL);
        if (accepted == INVALID_LISTEN_SOCK) break;

        if (s->count >= SOCKET_SERVER_MAX_CONNECTIONS) {
            CLOSESOCK(accepted);
            continue;
        }

        SocketConnection* conn = (SocketConnection*)malloc(sizeof(SocketConnection));
        SocketConnection_fromAccepted(conn, accepted);
        s->connections[s->count++] = conn;

        if (s->listener.clientConnected) s->listener.clientConnected(s->listener.ctx, conn);
    }

    for (int i = 0; i < s->count; ++i) {
        SocketConnection* conn = s->connections[i];

        if (!SocketConnection_isConnected(conn)) {
            // Client also holds this pointer and outlives the connection
            // briefly during its own cleanup, so it is not freed here.
            // Never exercised: multiplayer is unused in this version.
            s->connections[i] = s->connections[--s->count];
            i--;
            continue;
        }

        SocketConnection_tick(conn);
        if (!SocketConnection_isConnected(conn) && s->listener.clientException) {
            s->listener.clientException(s->listener.ctx, conn, "Disconnected");
        }
    }
}
