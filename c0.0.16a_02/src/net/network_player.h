// net/network_player.h: remote players
// every update queues a halfway step plus the real one,
// tick plays them back and render lerps between ticks

#ifndef NET_NETWORK_PLAYER_H
#define NET_NETWORK_PLAYER_H

#include "../entity.h"
#include "../gui/font.h"
#include <stdbool.h>

#define NETPLAYER_QUEUE_CAP 64

// a queued move, hasPos and hasRot say what the packet carried
typedef struct {
    float x, y, z;
    float yaw, pitch;
    bool hasPos, hasRot;
} NetPlayerMoveEntry;

typedef struct {
    Entity base;
    bool used;
    int id;
    char name[65];

    float animStep, animStepO;
    float yBodyRot, yBodyRotO;

    // raw 1/32 block position, kept separately so the deltas add up without float drift
    int xp, yp, zp;

    NetPlayerMoveEntry moveQueue[NETPLAYER_QUEUE_CAP];
    int queueHead, queueCount;
} NetworkPlayer;

void NetworkPlayer_init(NetworkPlayer* np, Level* level, int id, const char* name,
                         int xRaw, int yRaw, int zRaw, float yaw, float pitch);
void NetworkPlayer_onTick(NetworkPlayer* np);
// the name tag faces the local player's yRot
void NetworkPlayer_render(const NetworkPlayer* np, float partialTicks, float localPlayerYaw, Font* font);

// Teleport, raw 1/32 block shorts
void NetworkPlayer_teleport(NetworkPlayer* np, int xRaw, int yRaw, int zRaw, float yaw, float pitch);
// MoveAndLook, dx dy dz are the raw byte deltas
void NetworkPlayer_queueMoveLook(NetworkPlayer* np, int dx, int dy, int dz, float yaw, float pitch);
// Move, position only
void NetworkPlayer_queueMove(NetworkPlayer* np, int dx, int dy, int dz);
// Look, rotation only
void NetworkPlayer_queueLook(NetworkPlayer* np, float yaw, float pitch);

#endif
