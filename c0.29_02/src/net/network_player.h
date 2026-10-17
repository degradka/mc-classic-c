// net/network_player.h: remote players
// every update queues a halfway step plus the real one
// and aiStep plays them back instead of running any ai

#ifndef NET_NETWORK_PLAYER_H
#define NET_NETWORK_PLAYER_H

#include "../entity.h"
#include "../gui/font.h"
#include <stdbool.h>

typedef struct {
    float x, y, z;
    float yRot, xRot;
    bool rotating, moving;
} PlayerMove;

typedef struct {
    Entity e; // first member, the aiStep hook casts back from it
    // ConnectionManager.players holds it, and the level's entity list holds it.
    // a respawn under the same id leaves the old one in the level unmapped
    bool mapped, inLevel;
    int id;
    char name[65];        // color codes stripped
    char displayName[65];
    int xp, yp, zp;       // 1/32 block network position
    PlayerMove* moves;
    int moveHead, moveCount, moveCap;
} NetworkPlayer;

void NetworkPlayer_init(NetworkPlayer* np, Level* level, int id, const char* displayName,
                        int xRaw, int yRaw, int zRaw, float yaw, float pitch);
void NetworkPlayer_free(NetworkPlayer* np);
void NetworkPlayer_onTick(NetworkPlayer* np);
void NetworkPlayer_render(const NetworkPlayer* np, float partialTicks);
// the name tag, only for the player the crosshair is on, it faces the local player's yRot
void NetworkPlayer_renderHover(const NetworkPlayer* np, float partialTicks, float localPlayerYaw, Font* font);

void NetworkPlayer_teleport(NetworkPlayer* np, int xRaw, int yRaw, int zRaw, float yaw, float pitch);
void NetworkPlayer_queueMoveLook(NetworkPlayer* np, int dx, int dy, int dz, float yaw, float pitch);
void NetworkPlayer_queueMove(NetworkPlayer* np, int dx, int dy, int dz);
void NetworkPlayer_queueLook(NetworkPlayer* np, float yaw, float pitch);

#endif
