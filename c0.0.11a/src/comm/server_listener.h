// comm/server_listener.h

#ifndef COMM_SERVER_LISTENER_H
#define COMM_SERVER_LISTENER_H

struct SocketConnection;

typedef struct ServerListener {
    void (*clientConnected)(void *ctx, struct SocketConnection *conn);
    void (*clientException)(void *ctx, struct SocketConnection *conn, const char *message);
    void *ctx;
} ServerListener;

#endif /* COMM_SERVER_LISTENER_H */