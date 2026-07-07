// net/network_player.h: other players on the server
// every update queues a halfway step and the real one, the tick plays them back

#ifndef NET_NETWORK_PLAYER_H
#define NET_NETWORK_PLAYER_H

#include "../entity.h"
#include <stdbool.h>

typedef struct NetworkPlayer {
    Entity base;
    int id;
    char name[65];

    float animStep, animStepO;
    float yBodyRot, yBodyRotO;

    // position in 1/32 blocks, the deltas add up on these
    int xp, yp, zp;

    PositionUpdate* moveQueue;
    int queueCount, queueCapacity;
} NetworkPlayer;

void NetworkPlayer_init(NetworkPlayer* np, Level* level, int id, const char* name,
                        int x, int y, int z, float yRot, float xRot);
void NetworkPlayer_free(NetworkPlayer* np);
void NetworkPlayer_onTick(NetworkPlayer* np);
void NetworkPlayer_render(const NetworkPlayer* np, float a);

void NetworkPlayer_teleport(NetworkPlayer* np, int x, int y, int z, float yRot, float xRot);
void NetworkPlayer_queueMoveLook(NetworkPlayer* np, int dx, int dy, int dz, float yRot, float xRot);
void NetworkPlayer_queueMove(NetworkPlayer* np, int dx, int dy, int dz);
void NetworkPlayer_queueLook(NetworkPlayer* np, float yRot, float xRot);

#endif
