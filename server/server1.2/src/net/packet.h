// net/packet.h: the packet table, an id byte then big endian fields
// B byte, S short, T a 64 byte space padded utf-8 string, A a 1024 byte array
// the size checked before reading leaves the strings out, so a short packet underflows

#ifndef NET_PACKET_H
#define NET_PACKET_H

#include "../jstring.h"
#include <stdint.h>

enum {
    PACKET_LOGIN,
    PACKET_PING,
    PACKET_LEVEL_INIT,
    PACKET_LEVEL_CHUNK,
    PACKET_LEVEL_FINAL,
    PACKET_PLACE,
    PACKET_SET_TILE,
    PACKET_SPAWN,
    PACKET_TELEPORT,
    PACKET_MOVE_LOOK,
    PACKET_MOVE,
    PACKET_LOOK,
    PACKET_DESPAWN,
    PACKET_MESSAGE,
    PACKET_DISCONNECT,
    PACKET_COUNT
};

typedef struct {
    const char* fields;
    int size;
} PacketType;

extern const PacketType PACKETS[PACKET_COUNT];

typedef struct {
    int i;
    const JStr* s;
    const uint8_t* bytes;
    int len;
} PacketValue;

#endif
