// net/packet.c

#include "packet.h"

const int PacketPayloadLen[PACKET_COUNT] = {
    1 + 64 + 64,     // Login
    0,               // Ping
    0,               // LevelInit
    2 + 1024 + 1,    // LevelChunk
    2 + 2 + 2,       // LevelFinalize
    2 + 2 + 2 + 1 + 1, // SetBlock client to server
    2 + 2 + 2 + 1,   // SetBlock server to client
    1 + 64 + 2 + 2 + 2 + 1 + 1, // SpawnPlayer
    1 + 2 + 2 + 2 + 1 + 1,      // Teleport
    1 + 1 + 1 + 1 + 1 + 1,      // MoveAndLook
    1 + 1 + 1 + 1,   // Move
    1 + 1 + 1,       // Look
    1,               // DespawnPlayer
    1 + 64,          // Message
    64               // Disconnect
};
