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

    // the connect blocks, then the channel goes non blocking
    int connectResult = connect(sock, res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);
    if (connectResult != 0) {
        NetSocket_close(sock);
        return 0;
    }
#if defined(_WIN32)
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

    int yes = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&yes, sizeof yes);
    int no = 0;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char*)&no, sizeof no);
#if defined(__APPLE__)
    setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (const char*)&yes, sizeof yes);
#endif

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
    // the -1 at the end of the stream is ignored, only a failing write notices
    return n;
}

int NetSocket_write(sock_t sock, const void* buf, int len) {
#if defined(MSG_NOSIGNAL)
    int n = send(sock, (const char*)buf, len, MSG_NOSIGNAL);
#else
    int n = send(sock, (const char*)buf, len, 0);
#endif
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
