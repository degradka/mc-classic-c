// net/packet.h: c0.0.16a_02 wire protocol. Every packet is
// [1 id byte][fields, big endian]. String fields are always a fixed 64
// bytes, space padded. byte[] fields are always a fixed 1024 bytes.
//
// The client decompile's own registry (com.mojang.minecraft.net.a) only has
// 14 entries, ids 0-13, with no ping packet at all. But the real server1.2
// jar's registry (com.mojang.minecraft.a.a) has an extra zero-payload ping
// packet inserted at id 1, sent periodically from the main server loop and
// not just at connect time, which shifts every packet from LevelInit
// onward up by one id compared to the client-only reading. This must be a
// server revision that shipped slightly ahead of this client build. Since
// interop with the real server is the whole point, this table follows the
// server's numbering (confirmed against server1.2's actual send/receive
// call sites), not the client decompile's.

#ifndef NET_PACKET_H
#define NET_PACKET_H

enum {
    PACKET_LOGIN          = 0,  // byte protocolVersion, String username/serverName, String session/motd
    PACKET_PING           = 1,  // (no fields), sent periodically by the server, not just at connect
    PACKET_LEVEL_INIT     = 2,  // (no fields)
    PACKET_LEVEL_CHUNK    = 3,  // short length, byte[1024] chunk, byte percent
    PACKET_LEVEL_FINALIZE = 4,  // short width, short depth, short height (depth = vertical, height = horizontal, yes really)
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
    PACKET_COUNT          = 15
};

#define PACKET_STRING_LEN 64
#define PACKET_ARRAY_LEN  1024

// payload byte length after the leading id byte, indexed by packet id
extern const int PacketPayloadLen[PACKET_COUNT];

#endif
