// server/minecraft_server.c

#include "minecraft_server.h"
#include "client.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void on_client_connected(void *ctx, struct SocketConnection *conn) {
    MinecraftServer *s = (MinecraftServer*)ctx;
    if (s->count >= 16) return;
    Client *c = (Client*)malloc(sizeof(Client));
    if (!c) return;
    Client_init(c, s, conn);
    s->clients[s->count++] = c;
    (void)conn;
}

static void on_client_exception(void *ctx, struct SocketConnection *conn, const char *msg) {
    (void)conn; (void)msg;
    (void)ctx;
}

int MinecraftServer_init(MinecraftServer *s, const unsigned char ips[4], int port) {
    memset(s, 0, sizeof(*s));
    ServerListener l = { on_client_connected, on_client_exception, s };
    return SocketServer_init(&s->socketServer, ips, port, &l);
}

void MinecraftServer_tick(MinecraftServer *s) {
    SocketServer_tick(&s->socketServer);
}

void MinecraftServer_disconnect(MinecraftServer *s, Client *c) {
    if (!s || !c) return;
    for (int i = 0; i < s->count; ++i) {
        if (s->clients[i] == c) {
            s->clients[i] = s->clients[s->count - 1];
            s->count--;
            break;
        }
    }
    free(c);
}
