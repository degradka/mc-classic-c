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

static void ensureWinsockInit(void) {
#if defined(_WIN32)
    static int wsaInited = 0;
    if (!wsaInited) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        wsaInited = 1;
    }
#endif
}

static void setNonBlocking(sock_t sock) {
#if defined(_WIN32)
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

// matches com.mojang.a.a: binds a non-blocking ServerSocketChannel to the
// configured port
int NetSocket_listen(sock_t* out, int port) {
    ensureWinsockInit();

    sock_t sock = socket(AF_INET, SOCK_STREAM, 0);
#if defined(_WIN32)
    if (sock == INVALID_SOCKET) return 0;
#else
    if (sock < 0) return 0;
#endif

    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((unsigned short)port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof addr) != 0 || listen(sock, 16) != 0) {
        NetSocket_close(sock);
        return 0;
    }

    setNonBlocking(sock);
    *out = sock;
    return 1;
}

int NetSocket_accept(sock_t listenSock, sock_t* outClient) {
    sock_t client = accept(listenSock, NULL, NULL);
#if defined(_WIN32)
    if (client == INVALID_SOCKET) return 0; // WSAEWOULDBLOCK or a transient accept error: nothing pending
#else
    if (client < 0) return 0; // EWOULDBLOCK/EAGAIN or a transient accept error: nothing pending
#endif
    *outClient = client;
    return 1;
}

// matches com.mojang.a.b's constructor: TcpNoDelay=true, KeepAlive=false,
// then non-blocking for the rest of the connection's life
void NetSocket_configure(sock_t sock) {
    int yes = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&yes, sizeof yes);
    int no = 0;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char*)&no, sizeof no);
    setNonBlocking(sock);
}

void NetSocket_getRemoteAddress(sock_t sock, char* out, size_t outSize) {
    struct sockaddr_in addr;
    socklen_t len = sizeof addr;
    if (getpeername(sock, (struct sockaddr*)&addr, &len) != 0) {
        if (outSize > 0) out[0] = '\0';
        return;
    }
    const char* s = inet_ntoa(addr.sin_addr);
    snprintf(out, outSize, "%s", s ? s : "");
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
