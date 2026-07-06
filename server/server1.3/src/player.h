// player.h: one connected client, its login, the level send and the packets it sends

#ifndef PLAYER_H
#define PLAYER_H

#include "jexc.h"
#include "jstring.h"
#include "net/connection.h"
#include "net/packet.h"
#include <stdbool.h>
#include <stdint.h>

struct MinecraftServer;

// a packet that waits for the player to finish joining
typedef struct {
    int id;
    int ints[7];
    JStr* strings[7];
} QueuedPacket;

// a gzipped level from the send thread, kept until the player goes
typedef struct LevelBuffer {
    struct LevelBuffer* next;
    uint8_t* data;
    int len;
} LevelBuffer;

typedef struct Player {
    SocketConnection* conn;
    struct MinecraftServer* server;
    bool loggedIn;
    bool spawned;
    JStr name;
    int id;
    // NULL once it's been sent
    QueuedPacket* queue;
    int queueSize, queueCap;
    bool queueGone;
    // the last position and look the others were told, fixed point and 256ths of a turn
    int x, y, z, pitch, yaw;
    int moves;
    int64_t connectTime;
    LevelBuffer* volatile level;
    LevelBuffer* volatile buffers;
    volatile int threads;
} Player;

Player* Player_create(struct MinecraftServer* server, SocketConnection* conn, int id);
void Player_free(Player* p);
const char* Player_toString(const Player* p, char* buf, int size);
void Player_handle(Player* p, int id, const PacketValue* values, const bool* present);
void Player_kick(Player* p, const char* reason);
void Player_message(Player* p, const char* text);
void Player_tick(Player* p);
void Player_send(Player* p, int id, const PacketValue* values);
void Player_onException(Player* p, const Jcatch* e);

#endif
