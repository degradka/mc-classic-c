// net/net_socket.c

#include "net_socket.h"
#include "../jexc.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
  #include <ws2tcpip.h>
  #define WOULD_BLOCK(e) ((e) == WSAEWOULDBLOCK)
  static int lastError(void) { return WSAGetLastError(); }
#else
  #include <errno.h>
  #include <fcntl.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <sys/socket.h>
  #include <unistd.h>
  #define WOULD_BLOCK(e) ((e) == EWOULDBLOCK || (e) == EAGAIN)
  #define INVALID_SOCKET (-1)
  static int lastError(void) { return errno; }
#endif

// the system's text for an error, without the final period
static void errorText(int err, char* out, size_t size) {
#if defined(_WIN32)
    DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, (DWORD)err,
                             MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), out, (DWORD)size, NULL);
    if (n > 3) {
        if (out[n - 1] == '\n') n--;
        if (out[n - 1] == '\r') n--;
        if (out[n - 1] == '.') n--;
    }
    out[n] = '\0';
#else
    snprintf(out, size, "%s", strerror(err));
#endif
}

static void throwLast(void) {
    char text[256];
    errorText(lastError(), text, sizeof text);
    jthrowIO(text);
}

static void nonBlocking(sock_t sock) {
#if defined(_WIN32)
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
#endif
}

int NetSocket_listen(int port, sock_t* out, char* exception, size_t size) {
#if defined(_WIN32)
    static int started = 0;
    if (!started) {
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
        started = 1;
    }
#endif
    sock_t sock = socket(AF_INET, SOCK_STREAM, 0);
#if !defined(_WIN32)
    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);
#endif
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);
    if (sock == INVALID_SOCKET || bind(sock, (struct sockaddr*)&addr, sizeof addr) != 0 || listen(sock, 50) != 0) {
        char text[256];
        int err = lastError();
        errorText(err, text, sizeof text);
#if defined(_WIN32)
        // these winsock errors get their own words
        if (err == WSAEADDRINUSE) snprintf(text, sizeof text, "Address already in use");
        if (err == WSAEACCES) snprintf(text, sizeof text, "Permission denied");
        if (err == WSAEADDRNOTAVAIL) snprintf(text, sizeof text, "Cannot assign requested address");
        snprintf(exception, size, "java.net.BindException: %s: bind", text);
#else
        snprintf(exception, size, "java.net.BindException: %s", text);
#endif
        if (sock != INVALID_SOCKET) NetSocket_close(sock);
        return 0;
    }
    nonBlocking(sock);
    *out = sock;
    return 1;
}

int NetSocket_accept(sock_t listen, sock_t* out) {
    sock_t client = accept(listen, NULL, NULL);
    if (client == INVALID_SOCKET) {
        if (WOULD_BLOCK(lastError())) return 0;
        throwLast();
    }
    *out = client;
    return 1;
}

void NetSocket_configure(sock_t sock) {
    nonBlocking(sock);
    int yes = 1, no = 0, tos = 24;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&yes, sizeof yes);
    setsockopt(sock, IPPROTO_IP, IP_TOS, (const char*)&tos, sizeof tos);
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char*)&no, sizeof no);
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&no, sizeof no);
}

void NetSocket_address(sock_t sock, char* out, size_t size) {
    struct sockaddr_in addr;
    socklen_t len = sizeof addr;
    memset(&addr, 0, sizeof addr);
    getpeername(sock, (struct sockaddr*)&addr, &len);
    unsigned char* b = (unsigned char*)&addr.sin_addr;
    snprintf(out, size, "/%d.%d.%d.%d", b[0], b[1], b[2], b[3]);
}

int NetSocket_read(sock_t sock, void* buf, int len) {
    if (len <= 0) return 0;
    int n = (int)recv(sock, (char*)buf, len, 0);
    if (n < 0) {
        if (WOULD_BLOCK(lastError())) return 0;
        throwLast();
    }
    return n == 0 ? -1 : n;
}

int NetSocket_write(sock_t sock, const void* buf, int len) {
    if (len <= 0) return 0;
#if defined(MSG_NOSIGNAL)
    int n = (int)send(sock, (const char*)buf, len, MSG_NOSIGNAL);
#else
    int n = (int)send(sock, (const char*)buf, len, 0);
#endif
    if (n < 0) {
        if (WOULD_BLOCK(lastError())) return 0;
        throwLast();
    }
    return n;
}

void NetSocket_close(sock_t sock) {
#if defined(_WIN32)
    closesocket(sock);
#else
    close(sock);
#endif
}
