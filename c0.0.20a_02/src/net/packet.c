// net/packet.c

#include "packet.h"

const int PacketFullLen[PACKET_COUNT] = {
    1 + 64 + 64 + 1,
    0,
    0,
    2 + 1024 + 1,
    2 + 2 + 2,
    2 + 2 + 2 + 1 + 1,
    2 + 2 + 2 + 1,
    1 + 64 + 2 + 2 + 2 + 1 + 1,
    1 + 2 + 2 + 2 + 1 + 1,
    1 + 1 + 1 + 1 + 1 + 1,
    1 + 1 + 1 + 1,
    1 + 1 + 1,
    1,
    1 + 64,
    64
};
