// net/net_socket.c

#include "net_socket.h"
#include <string.h>
#include <stdio.h>

#if defined(_WIN32)
  #include <ws2tcpip.h>
#else
  #include <sys/socket.h>
  #include <sys/select.h>
  #include <sys/time.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
#endif

int NetSocket_connect(sock_t* out, const char* host, int port) {
#if defined(_WIN32)
    static int wsaInited = 0;
    if (!wsaInited) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 0;
        wsaInited = 1;
    }
#endif

    char portStr[16];
    snprintf(portStr, sizeof portStr, "%d", port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = NULL;
    if (getaddrinfo(host, portStr, &hints, &res) != 0 || !res) return 0;

    sock_t sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
#if defined(_WIN32)
    if (sock == INVALID_SOCKET) { freeaddrinfo(res); return 0; }
#else
    if (sock < 0) { freeaddrinfo(res); return 0; }
#endif

    // switch to non-blocking BEFORE connecting, so an unreachable host
    // times out in a few seconds instead of hanging the whole window for
    // however long the OS's own default TCP connect timeout is (can be a
    // minute or more for a silently dropped connection). Not what the real
    // client does -- it blocks unconditionally -- but freezing indefinitely
    // is a worse experience than one architectural deviation here
#if defined(_WIN32)
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

    int connectResult = connect(sock, res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);

    int connected = 0;
    if (connectResult == 0) {
        connected = 1; // completed immediately, e.g. localhost
    } else {
#if defined(_WIN32)
        int inProgress = (WSAGetLastError() == WSAEWOULDBLOCK);
#else
        int inProgress = (errno == EINPROGRESS);
#endif
        if (inProgress) {
            fd_set writeSet;
            FD_ZERO(&writeSet);
            FD_SET(sock, &writeSet);
            struct timeval tv;
            tv.tv_sec = 8;
            tv.tv_usec = 0;
            if (select((int)sock + 1, NULL, &writeSet, NULL, &tv) > 0) {
                int soErr = 0;
                socklen_t errLen = sizeof soErr;
                getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&soErr, &errLen);
                connected = (soErr == 0);
            }
        }
    }

    if (!connected) {
        NetSocket_close(sock);
        return 0;
    }

    int yes = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&yes, sizeof yes);
    int no = 0;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char*)&no, sizeof no);

    *out = sock;
    return 1;
}

void NetSocket_close(sock_t sock) {
#if defined(_WIN32)
    closesocket(sock);
#else
    close(sock);
#endif
}

int NetSocket_read(sock_t sock, void* buf, int len) {
    int n = recv(sock, (char*)buf, len, 0);
    if (n < 0) {
#if defined(_WIN32)
        if (WSAGetLastError() == WSAEWOULDBLOCK) return 0;
#else
        if (errno == EWOULDBLOCK || errno == EAGAIN) return 0;
#endif
        return -1;
    }
    if (n == 0) return -1; // orderly remote close
    return n;
}

int NetSocket_write(sock_t sock, const void* buf, int len) {
    int n = send(sock, (const char*)buf, len, 0);
    if (n < 0) {
#if defined(_WIN32)
        if (WSAGetLastError() == WSAEWOULDBLOCK) return 0;
#else
        if (errno == EWOULDBLOCK || errno == EAGAIN) return 0;
#endif
        return -1;
    }
    return n;
}
