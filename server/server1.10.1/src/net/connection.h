// net/connection.h: a client's socket and its read and write buffers, with ByteBuffer rules

#ifndef NET_CONNECTION_H
#define NET_CONNECTION_H

#include "net_socket.h"
#include "packet.h"
#include "../jstring.h"
#include <stdbool.h>
#include <stdint.h>

struct Player;

typedef struct {
    uint8_t* data;
    int pos, lim, cap;
} ByteBuffer;

typedef struct SocketConnection {
    volatile bool connected;
    sock_t sock;
    bool hasSocket;
    ByteBuffer in, out;
    struct Player* player;
    JStr address;
    bool registered;
    int refs;
} SocketConnection;

void BB_flip(ByteBuffer* b);
void BB_compact(ByteBuffer* b);

SocketConnection* Conn_create(sock_t sock);
void Conn_free(SocketConnection* c);
void Conn_close(SocketConnection* c);
void Conn_flush(SocketConnection* c);
void Conn_send(SocketConnection* c, int id, const PacketValue* values);
// false is null, after an exception or once closed
bool Conn_read(SocketConnection* c, char type, PacketValue* out, JStr* string, uint8_t* bytes);

#endif
