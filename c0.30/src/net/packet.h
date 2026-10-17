// net/packet.h: an id byte then big endian fields
// strings are 64 bytes space padded, byte arrays 1024

#ifndef NET_PACKET_H
#define NET_PACKET_H

enum {
    PACKET_LOGIN          = 0,  // byte protocolVersion, String username/serverName, String session/motd, byte userType
    PACKET_PING           = 1,  // (no fields), the server sends it now and then
    PACKET_LEVEL_INIT     = 2,  // (no fields)
    PACKET_LEVEL_CHUNK    = 3,  // short length, byte[1024] chunk, byte percent
    PACKET_LEVEL_FINALIZE = 4,  // short width, short depth, short height, depth is the vertical one
    PACKET_SET_BLOCK_CS   = 5,  // short x,y,z, byte mode, byte type, client to server only
    PACKET_SET_BLOCK_SC   = 6,  // short x,y,z, byte type
    PACKET_SPAWN_PLAYER   = 7,  // byte id, String name, short x,y,z, byte yaw, byte pitch
    PACKET_TELEPORT       = 8,  // byte id, short x,y,z, byte yaw, byte pitch
    PACKET_MOVE_LOOK      = 9,  // byte id, byte dx,dy,dz, byte yaw, byte pitch
    PACKET_MOVE           = 10, // byte id, byte dx,dy,dz
    PACKET_LOOK           = 11, // byte id, byte yaw, byte pitch
    PACKET_DESPAWN_PLAYER = 12, // byte id
    PACKET_MESSAGE        = 13, // byte senderId, String text
    PACKET_DISCONNECT     = 14, // String reason
    PACKET_SET_USER_TYPE  = 15, // byte userType, 100 is an op
    PACKET_COUNT          = 16
};

#define PACKET_STRING_LEN 64
#define PACKET_ARRAY_LEN  1024

// payload byte length after the leading id byte, indexed by packet id
extern const int PacketPayloadLen[PACKET_COUNT];

#endif
