// comm/socket_connection.c

#include "socket_connection.h"
#include <string.h>
#include <time.h>

static uint64_t now_ms(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)(ts.tv_nsec / 1000000ull);
}

int SocketConnection_init(SocketConnection *c, const char *ip, int port) {
    (void)ip; (void)port;
    memset(c, 0, sizeof(*c));
    c->connected = 1;
    c->lastReadMillis = now_ms();
    return 1;
}

int SocketConnection_initFromHandle(SocketConnection *c, int fake_handle) {
    memset(c, 0, sizeof(*c));
    c->connected = 1;
    c->placeholder_fd = fake_handle;
    c->lastReadMillis = now_ms();
    return 1;
}

void SocketConnection_disconnect(SocketConnection *c) {
    if (!c) return;
    c->connected = 0;
    c->readLen = c->writeLen = 0;
}

int SocketConnection_isConnected(const SocketConnection *c) {
    return c && c->connected;
}

void SocketConnection_setListener(SocketConnection *c,
                                  const ConnectionListener *listener,
                                  void *ctx)
{
    if (!c) return;
    if (listener) c->listener = *listener;
    c->listener_ctx = ctx;
}

unsigned char *SocketConnection_getWriteBuffer(SocketConnection *c, int *capacity) {
    if (capacity) *capacity = SOCKETCONN_BUFFER_SIZE - c->writeLen;
    return c->writeBuffer + c->writeLen;
}

/* No real sockets yet: just loopback anything written into readBuffer
   and issue a single command callback per tick (cmd = first byte or 0). */
int SocketConnection_tick(SocketConnection *c) {
    if (!c || !c->connected) return 0;

    // "send" -> move write data into read side, clipped to capacity
    int can = SOCKETCONN_BUFFER_SIZE - c->readLen;
    int n   = (c->writeLen < can) ? c->writeLen : can;
    if (n > 0) {
        memcpy(c->readBuffer + c->readLen, c->writeBuffer, (size_t)n);
        c->readLen  += n;
        // shift remaining write bytes to front
        memmove(c->writeBuffer, c->writeBuffer + n, (size_t)(c->writeLen - n));
        c->writeLen -= n;
    }

    // If we have read data, synthesize a callback
    if (c->readLen > 0 && c->listener.command) {
        unsigned char cmd = c->readBuffer[0];
        c->listener.command(c->listener_ctx, cmd, c->readLen, c->readBuffer, c->readLen);
    }

    c->lastReadMillis = now_ms();
    return 1;
}

int SocketConnection_getSentBytes(const SocketConnection *c) { (void)c; return 0; }
int SocketConnection_getReadBytes(const SocketConnection *c) { return c ? c->readLen : 0; }
void SocketConnection_clearSentBytes(SocketConnection *c)    { (void)c; }
void SocketConnection_clearReadBytes(SocketConnection *c)    { if (c) c->readLen = 0; }