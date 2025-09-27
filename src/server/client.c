// server/client.c

#include "client.h"
#include "minecraft_server.h"
#include <string.h>

static void on_cmd(void *ctx, unsigned char cmd, int remaining,
                   const unsigned char *data, int len)
{
    (void)ctx; (void)cmd; (void)remaining; (void)data; (void)len;
}

static void on_exc(void *ctx, const char *message) {
    Client *c = (Client*)ctx;
    (void)message;
    Client_disconnect(c);
}

void Client_init(Client *c, struct MinecraftServer *server, SocketConnection *conn) {
    memset(c, 0, sizeof(*c));
    c->server = server;
    c->conn   = conn;

    c->listener.command        = on_cmd;
    c->listener.handleException= on_exc;

    SocketConnection_setListener(conn, &c->listener, c);
}

void Client_disconnect(Client *c) {
    if (!c) return;
    if (c->server) MinecraftServer_disconnect(c->server, c);
    if (c->conn)   SocketConnection_disconnect(c->conn);
}
