// server/minecraft_server.c

#include "minecraft_server.h"
#include "client.h"
#include "../timer.h"
#include <stdlib.h>
#include <string.h>

static Client* findClient(MinecraftServer* s, const SocketConnection* conn) {
    for (int i = 0; i < s->count; ++i) {
        if (s->clients[i]->conn == conn) return s->clients[i];
    }
    return NULL;
}

static void on_client_connected(void* ctx, struct SocketConnection* conn) {
    MinecraftServer* s = (MinecraftServer*)ctx;
    if (s->count >= SOCKET_SERVER_MAX_CONNECTIONS) return;
    Client* c = (Client*)malloc(sizeof(Client));
    if (!c) return;
    Client_init(c, s, conn);
    s->clients[s->count++] = c;
}

static void on_client_exception(void* ctx, struct SocketConnection* conn, const char* msg) {
    MinecraftServer* s = (MinecraftServer*)ctx;
    Client* c = findClient(s, conn);
    if (c && c->listener.handleException) c->listener.handleException(c, msg);
}

int MinecraftServer_init(MinecraftServer* s, const unsigned char ips[4], int port) {
    memset(s, 0, sizeof(*s));
    ServerListener l = { on_client_connected, on_client_exception, s };
    return SocketServer_init(&s->socketServer, ips, port, l);
}

void MinecraftServer_tick(MinecraftServer* s) {
    SocketServer_tick(&s->socketServer);
}

void MinecraftServer_disconnect(MinecraftServer* s, Client* c) {
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

void MinecraftServer_run(MinecraftServer* s) {
    for (;;) {
        MinecraftServer_tick(s);
        sleepMillis(5);
    }
}
