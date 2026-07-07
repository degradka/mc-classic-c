// net/network_player.h: remote player entity, ported from
// com.mojang.minecraft.net.NetworkPlayer. Position/rotation are never set
// directly from incoming packets -- every update enqueues a synthetic
// halfway waypoint plus the real final state onto a per-player move queue,
// so even a single coarse network update gets a 2 step interpolation path.
// tick() drains the queue on top of that, and render() does its own
// partial-tick lerp between previous/current tick state, giving two
// independent smoothing layers.

#ifndef NET_NETWORK_PLAYER_H
#define NET_NETWORK_PLAYER_H

#include "../entity.h"
#include "../gui/font.h"
#include <stdbool.h>

#define NETPLAYER_QUEUE_CAP 64

// matches net.b: a queued waypoint. hasPos/hasRot mirror which fields this
// particular update actually carries (Move has no rotation, Look has no
// position, MoveAndLook/Teleport/SpawnPlayer have both)
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

    // raw 1/32 block fixed point network position, tracked independently of
    // base.x/y/z so relative Move/MoveAndLook deltas accumulate exactly like
    // the real source instead of drifting through float rounding
    int xp, yp, zp;

    NetPlayerMoveEntry moveQueue[NETPLAYER_QUEUE_CAP];
    int queueHead, queueCount;
} NetworkPlayer;

void NetworkPlayer_init(NetworkPlayer* np, Level* level, int id, const char* name,
                         int xRaw, int yRaw, int zRaw, float yaw, float pitch);
void NetworkPlayer_onTick(NetworkPlayer* np);
// localPlayerYaw billboards the name tag to face the camera, matching the
// real source rotating it by -Minecraft.player.yRot
void NetworkPlayer_render(const NetworkPlayer* np, float partialTicks, float localPlayerYaw, Font* font);

// matches NetworkPlayer.teleport(short,short,short,float,float): incoming
// Teleport packet, xRaw/yRaw/zRaw are the packet's raw 1/32 fixed point shorts
void NetworkPlayer_teleport(NetworkPlayer* np, int xRaw, int yRaw, int zRaw, float yaw, float pitch);
// matches NetworkPlayer.queue(byte,byte,byte,float,float): MoveAndLook,
// dx/dy/dz are the packet's raw signed byte deltas
void NetworkPlayer_queueMoveLook(NetworkPlayer* np, int dx, int dy, int dz, float yaw, float pitch);
// matches NetworkPlayer.queue(byte,byte,byte): Move, position only
void NetworkPlayer_queueMove(NetworkPlayer* np, int dx, int dy, int dz);
// matches NetworkPlayer.queue(float,float): Look, rotation only
void NetworkPlayer_queueLook(NetworkPlayer* np, float yaw, float pitch);

#endif
