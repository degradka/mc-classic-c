// comm/socket_connection.c

#include "socket_connection.h"
#include <string.h>
#include <time.h>

#if defined(_WIN32)
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  #define SOCK_ERR()      WSAGetLastError()
  #define WOULD_BLOCK(e)  ((e) == WSAEWOULDBLOCK)
  #define CLOSESOCK(s)    closesocket(s)
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  #define INVALID_SOCKET (-1)
  #define SOCK_ERR()      errno
  #define WOULD_BLOCK(e)  ((e) == EWOULDBLOCK || (e) == EAGAIN)
  #define CLOSESOCK(s)    close(s)
#endif

static void ensureSocketsInited(void) {
#if defined(_WIN32)
    static int inited = 0;
    if (!inited) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        inited = 1;
    }
#endif
}

static void setNonBlocking(sock_t s) {
#if defined(_WIN32)
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}

static long long nowMillis(void) {
    return (long long)time(NULL) * 1000;
}

int SocketConnection_open(SocketConnection* c, const char* ip, int port) {
    memset(c, 0, sizeof(*c));
    ensureSocketsInited();

    c->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (c->sock == INVALID_SOCKET) return 0;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        CLOSESOCK(c->sock);
        return 0;
    }

    // connect while still blocking, then switch to non blocking for tick(),
    // matching socketChannel.connect() followed by configureBlocking(false)
    if (connect(c->sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        CLOSESOCK(c->sock);
        return 0;
    }
    setNonBlocking(c->sock);

    c->connected = 1;
    c->lastRead = nowMillis();
    return 1;
}

void SocketConnection_fromAccepted(SocketConnection* c, sock_t sock) {
    memset(c, 0, sizeof(*c));
    c->sock = sock;
    setNonBlocking(c->sock);
    c->connected = 1;
    c->lastRead = nowMillis();
}

void SocketConnection_setListener(SocketConnection* c, ConnectionListener listener, void* ctx) {
    c->listener = listener;
    c->listenerCtx = ctx;
}

int SocketConnection_isConnected(const SocketConnection* c) {
    return c->connected;
}

void SocketConnection_disconnect(SocketConnection* c) {
    if (!c->connected) return;
    c->connected = 0;
    CLOSESOCK(c->sock);
}

unsigned char* SocketConnection_getBuffer(SocketConnection* c, int* capacity) {
    if (capacity) *capacity = SOCKETCONN_BUFFER_SIZE - c->writeLen;
    return c->writeBuffer + c->writeLen;
}

void SocketConnection_advanceWrite(SocketConnection* c, int n) {
    c->writeLen += n;
}

void SocketConnection_tick(SocketConnection* c) {
    if (!c->connected) return;

    if (c->writeLen > 0) {
        int n = send(c->sock, (const char*)c->writeBuffer, c->writeLen, 0);
        if (n > 0) {
            memmove(c->writeBuffer, c->writeBuffer + n, (size_t)(c->writeLen - n));
            c->writeLen -= n;
            c->totalBytesWritten += n;
        } else if (n < 0 && !WOULD_BLOCK(SOCK_ERR())) {
            SocketConnection_disconnect(c);
            return;
        }
    }

    int room = SOCKETCONN_BUFFER_SIZE - c->readLen;
    if (room > 0) {
        int n = recv(c->sock, (char*)c->readBuffer + c->readLen, room, 0);
        if (n > 0) {
            c->readLen += n;
            c->bytesRead += n;
        } else if (n == 0) {
            SocketConnection_disconnect(c);
            return;
        } else if (!WOULD_BLOCK(SOCK_ERR())) {
            SocketConnection_disconnect(c);
            return;
        }
    }

    if (c->readLen > 0 && c->listener.command) {
        c->listener.command(c->listenerCtx, c->readBuffer[0], c->readLen, c->readBuffer, c->readLen);
    }

    c->lastRead = nowMillis();
}

int  SocketConnection_getSentBytes(const SocketConnection* c) { return c->totalBytesWritten; }
int  SocketConnection_getReadBytes(const SocketConnection* c) { return c->bytesRead; }
void SocketConnection_clearSentBytes(SocketConnection* c)     { c->totalBytesWritten = 0; }
void SocketConnection_clearReadBytes(SocketConnection* c)     { c->bytesRead = 0; }
