// mob_ai.h: AI strategy objects mobs use to decide movement and attacks each
// tick, matching the real source's new mob/a/ package: base class `a`, plain
// wander class `c`, chase class `b` which is `c` plus a player seeking
// override. Collapsed into one struct plus a chase flag rather than a small
// class hierarchy, since there are only ever two concrete behaviors in this
// version. Player has no Ai at all, its own movement stays key driven, see
// player.c. Only hostile or passive mobs own one of these.

#ifndef MOB_AI_H
#define MOB_AI_H

#include "entity.h"
#include <stdbool.h>

struct Ai {
    // head pitch used while just wandering (not currently chasing/facing a
    // target), matches class a's `public int a` field: 0 for Pig/default
    // Mob, 30 for Zombie/Skeleton, 45 for Creeper
    int wanderPitch;
    // true selects class b's behavior: wander, then rotate to face and melee
    // the level's own player when in range. False is plain class c wander
    bool chase;

    // per tick strategy/movement state, matches class c's own fields
    float speedX, speedZ;  // c, d: current strafe/forward travel speed
    float turnSpeed;       // h: current yaw drift per tick
    bool  wantsJump;        // g
    int   attackCooldown;   // i: ticks left before another melee attempt

    // fired once from Mob_onTick right before a dead mob using this Ai gets
    // removed, matches class a's own a() override point, for example
    // Creeper's death explosion. NULL for anything with no special removal
    // behavior
    void (*onDeathTimeout)(Ai* ai, Entity* mob);
};

// zeroes wander/chase state (matches class c's constructor: fresh Random,
// wantsJump=false, attackCooldown=0). Call once after allocating an Ai
void Ai_init(Ai* ai, bool chase, int wanderPitch);

// per tick AI driver, matches class c's own a(Level,Mob): advances the
// wander or chase behavior, applies the water, lava, and jump logic, decays
// speed and turn, and calls Mob_travel to actually move. Freezes to fully
// still once the mob is dead, matching the real source's health<=0 branch.
// Call this from Mob_onTick when mob->ai is non NULL, not directly
void Ai_tick(Ai* ai, Entity* mob);

#endif
