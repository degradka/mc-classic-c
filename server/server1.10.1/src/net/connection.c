// net/connection.c

#include "connection.h"
#include "../player.h"
#include "../jexc.h"
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 0x100000

void BB_flip(ByteBuffer* b) {
    b->lim = b->pos;
    b->pos = 0;
}

void BB_compact(ByteBuffer* b) {
    memmove(b->data, b->data + b->pos, (size_t)(b->lim - b->pos));
    b->pos = b->lim - b->pos;
    b->lim = b->cap;
}

static void bufferInit(ByteBuffer* b) {
    b->data = (uint8_t*)malloc(BUFFER_SIZE);
    b->pos = 0;
    b->lim = b->cap = BUFFER_SIZE;
}

SocketConnection* Conn_create(sock_t sock) {
    SocketConnection* c = (SocketConnection*)calloc(1, sizeof *c);
    c->sock = sock;
    c->hasSocket = true;
    NetSocket_configure(sock);
    c->connected = true;
    bufferInit(&c->in);
    bufferInit(&c->out);
    char address[64];
    NetSocket_address(sock, address, sizeof address);
    jstr_set(&c->address, address);
    return c;
}

void Conn_free(SocketConnection* c) {
    free(c->in.data);
    free(c->out.data);
    free(c);
}

void Conn_flush(SocketConnection* c) {
    if (c->out.pos <= 0) return;
    BB_flip(&c->out);
    if (!c->hasSocket) jthrowNull();
    c->out.pos += NetSocket_write(c->sock, c->out.data + c->out.pos, c->out.lim - c->out.pos);
    BB_compact(&c->out);
}

void Conn_close(SocketConnection* c) {
    Jcatch e;
    JTRY(e) {
        Conn_flush(c);
        JEND(e);
    }
    c->connected = false;
    if (c->hasSocket) NetSocket_close(c->sock);
    c->hasSocket = false;
}

static void put(ByteBuffer* b, const uint8_t* v, int n) {
    if (b->lim - b->pos < n) jthrow(JEX_OVERFLOW, "java.nio.BufferOverflowException");
    memcpy(b->data + b->pos, v, (size_t)n);
    b->pos += n;
}

static void putField(SocketConnection* c, char type, const PacketValue* v) {
    uint8_t buf[64];
    if (type == 'B') {
        buf[0] = (uint8_t)v->i;
        put(&c->out, buf, 1);
    } else if (type == 'S') {
        buf[0] = (uint8_t)(v->i >> 8);
        buf[1] = (uint8_t)v->i;
        put(&c->out, buf, 2);
    } else if (type == 'T') {
        static uint8_t utf[JSTR_MAX * 3];
        int n = jstr_toUtf8(v->s, utf, sizeof utf);
        memset(buf, ' ', 64);
        memcpy(buf, utf, (size_t)(n < 64 ? n : 64));
        put(&c->out, buf, 64);
    } else if (type == 'A') {
        // Arrays.copyOf to 1024 when it's shorter
        static uint8_t padded[1024];
        if (v->len < 1024) {
            memset(padded, 0, sizeof padded);
            memcpy(padded, v->bytes, (size_t)v->len);
            put(&c->out, padded, 1024);
        } else {
            put(&c->out, v->bytes, v->len);
        }
    }
}

void Conn_send(SocketConnection* c, int id, const PacketValue* values) {
    if (!c->connected) return;
    uint8_t b = (uint8_t)id;
    put(&c->out, &b, 1);
    const char* fields = PACKETS[id].fields;
    for (int i = 0; fields[i]; ++i) {
        if (!c->connected) continue;
        Jcatch e;
        JTRY(e) {
            putField(c, fields[i], &values[i]);
            JEND(e);
        } else {
            if (!c->player) jthrowNull();
            Player_onException(c->player, &e);
        }
    }
}

static void get(ByteBuffer* b, uint8_t* v, int n) {
    if (b->lim - b->pos < n) jthrow(JEX_UNDERFLOW, "java.nio.BufferUnderflowException");
    memcpy(v, b->data + b->pos, (size_t)n);
    b->pos += n;
}

bool Conn_read(SocketConnection* c, char type, PacketValue* out, JStr* string, uint8_t* bytes) {
    if (!c->connected) return false;
    Jcatch e;
    JTRY(e) {
        uint8_t buf[64];
        if (type == 'B') {
            get(&c->in, buf, 1);
            out->i = (int8_t)buf[0];
        } else if (type == 'S') {
            get(&c->in, buf, 2);
            out->i = (int16_t)((buf[0] << 8) | buf[1]);
        } else if (type == 'T') {
            get(&c->in, buf, 64);
            jstr_fromUtf8(string, buf, 64);
            jstr_trim(string);
            out->s = string;
        } else if (type == 'A') {
            get(&c->in, bytes, 1024);
            out->bytes = bytes;
            out->len = 1024;
        }
        JEND(e);
        return true;
    }
    if (!c->player) jthrowNull();
    Player_onException(c->player, &e);
    return false;
}
