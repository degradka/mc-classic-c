// comm/socket_server.c

#include "socket_server.h"
#include <string.h>

int SocketServer_init(SocketServer *s, const unsigned char ips[4], int port,
                      const ServerListener *listener)
{
    memset(s, 0, sizeof(*s));
    memcpy(s->ips, ips, 4);
    s->port = port;
    if (listener) s->listener = *listener;
    return 1;
}

void SocketServer_tick(SocketServer *s) {
    for (int i = 0; i < s->count; ++i) {
        SocketConnection *c = &s->conns[i];
        if (!SocketConnection_isConnected(c)) {
            if (s->listener.clientException) {
                s->listener.clientException(s->listener.ctx, c, "Disconnected");
            }
            continue;
        }
        SocketConnection_tick(c);
    }
}