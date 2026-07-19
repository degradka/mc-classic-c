// item/arrow.h: fired projectile entity (c0.24_st_03), matches item/Arrow.java.
// Singleplayer only in the real source (gated on no active connection, see
// the comment at its spawn site in minecraft.c); fully self contained,
// does its own small step incremental collision rather than using
// Entity_move/Mob_travel like everything else

#ifndef ARROW_H
#define ARROW_H

#include "../entity.h"

typedef struct {
    Entity e;
    bool hasHit;
    int stickTime;
    int time;
    Entity* owner; // the shooter, for kill-score credit (Entity.killCredit) and playerTouch's own ownership check
    // 0 for a player shot arrow (or anything else explicitly credited to the
    // player, such as a Skeleton's own death scatter), 1 for anything else.
    // Drives the render texture's own row offset and how long a stuck arrow
    // lingers before vanishing on its own, matches Arrow's own type field
    int type;
    int damage; // 7 for type 0, 3 otherwise, matches Arrow's own damage field
    float gravity; // 1/speed, scales the per tick fall acceleration once in flight

    // c0.25_05_st: matches TakeEntityAnim's own 3 tick streak-to-player
    // animation, applied directly to this Arrow rather than a second entity,
    // the same pattern already used by Item_startPickup
    bool pickedUp;
    int pickupTime;
    float pickupOrgX, pickupOrgY, pickupOrgZ;
    Entity* pickupTarget;
} Arrow;

// yaw/pitch in degrees, matching the shooter's own current look direction.
// speed is Arrow's own constructor speed argument: 1.2 for the player's own
// shot, 1.0 for a Skeleton's aimed shot, 0.4 for its death scatter
void Arrow_init(Arrow* a, Level* level, Entity* owner, float x, float y, float z, float yaw, float pitch, float speed);
void Arrow_onTick(Arrow* a);
void Arrow_render(const Arrow* a, float partialTicks);
// matches Arrow.playerTouch(Player): starts the pickup animation, called
// once the touching player is confirmed eligible (stuck, this same player
// is owner, and under the carry cap), see Minecraft_checkArrowPickups
void Arrow_startPickup(Arrow* a, Entity* target);

#endif
