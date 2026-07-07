// net/packet.c

#include "packet.h"

const PacketType PACKETS[PACKET_COUNT] = {
    { "BTT", 1 },
    { "", 0 },
    { "", 0 },
    { "SAB", 2 + 1024 + 1 },
    { "SSS", 6 },
    { "SSSBB", 8 },
    { "SSSB", 7 },
    { "BTSSSBB", 9 },
    { "BSSSBB", 9 },
    { "BBBBBB", 6 },
    { "BBBB", 4 },
    { "BBB", 3 },
    { "B", 1 },
    { "BT", 1 },
    { "T", 0 }
};
