// comm/connection_listener.h

#ifndef COMM_CONNECTION_LISTENER_H
#define COMM_CONNECTION_LISTENER_H

#include <stddef.h>

typedef struct ConnectionListener ConnectionListener;

struct ConnectionListener {
    void (*handleException)(void *ctx, const char *message);
    void (*command)(void *ctx, unsigned char cmd, int remaining,
                    const unsigned char *data, int len);
};

#endif /* COMM_CONNECTION_LISTENER_H */