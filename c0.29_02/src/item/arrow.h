// item/arrow.h: arrows do their own collision instead of Entity_move

#ifndef ARROW_H
#define ARROW_H

#include "../entity.h"

typedef struct {
    Entity e;
    bool hasHit;
    int stickTime;
    int time;
    Entity* owner; // the shooter, for the kill score and the pickup check
    // 0 for player arrows, 1 for anything else
    int type;
    int damage; // 7 for the player's arrows, 3 otherwise
    float gravity; // 1/speed, scales the per tick fall acceleration once in flight

    // TakeEntityAnim folded in, like Item does
    bool pickedUp;
    int pickupTime;
    float pickupOrgX, pickupOrgY, pickupOrgZ;
    Entity* pickupTarget;
} Arrow;

// speed is 1.2 for the player, 1.0 for a skeleton shot, 0.4 for its death scatter
void Arrow_init(Arrow* a, Level* level, Entity* owner, float x, float y, float z, float yaw, float pitch, float speed);
void Arrow_onTick(Arrow* a);
void Arrow_render(const Arrow* a, float partialTicks);
// Arrow.playerTouch, the caller already checked it's stuck, owned and under the cap
void Arrow_startPickup(Arrow* a, Entity* target);

#endif
