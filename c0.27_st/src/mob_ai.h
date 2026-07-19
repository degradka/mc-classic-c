// mob_ai.h: AI strategy objects mobs use to decide movement and attacks each
// tick, matching the real source's mob/ai/ package: base class AI (empty),
// BasicAI (wander plus idle despawn), BasicAttackAI (extends BasicAI, adds a
// persistent locked attack target). Collapsed into one struct plus a chase
// flag rather than a small class hierarchy, since there are only ever two
// concrete behaviors (wander only, or wander plus attack) in this version,
// with a couple of optional callback hooks standing in for the handful of
// per species method overrides (Skeleton shooting instead of always
// meleeing, Creeper's self damage on a successful hit). Player has no Ai at
// all, its own movement stays key driven, see player.c. Only hostile or
// passive mobs own one of these.

#ifndef MOB_AI_H
#define MOB_AI_H

#include "entity.h"
#include <stdbool.h>

struct Ai {
    // head pitch used while just wandering, not currently locked onto a
    // target, matches AI's own defaultLookAngle field: 0 for Pig and
    // Skeleton (Skeleton's own ctor replaces Zombie's throwaway AI instance
    // with a fresh one that never sets this, so it keeps AI's own 0
    // default), 30 for Zombie, 45 for Creeper
    int wanderPitch;
    // true selects BasicAttackAI's behavior: wander, then acquire, track,
    // and melee (or, per species, shoot) a locked target. False is plain
    // BasicAI wander only, used by Pig
    bool chase;

    // matches BasicAI's own runSpeed field: how fast the wander step and a
    // locked chase both move, 0.7 default, 1.0 for Zombie, 0.3 for Skeleton
    float runSpeed;
    // matches BasicAttackAI's own damage field: the base melee damage
    // constant fed into the (rand+rand)/2*damage+1 roll, 6 default, 8 for
    // Skeleton
    int damage;

    // per tick strategy and movement state, matches BasicAI's own fields
    float speedX, speedZ;  // xxa, yya: current strafe/forward travel speed
    float turnSpeed;        // yRotA: current yaw drift per tick
    bool  wantsJump;         // jumping
    int   attackCooldown;    // attackDelay: ticks left before another melee attempt
    // matches BasicAI's own noActionTime: ticks since last taking damage or
    // being reset by a landed attack. Past 600 ticks, a small chance per
    // tick despawns the mob if it's also far from the player
    int   noActionTime;
    // matches BasicAttackAI's own attackTarget: once acquired (player within
    // 16 blocks), stays locked (even through line of sight breaks, only
    // dropped past 32 blocks with a 1% chance per tick, or once the target
    // is actually removed) rather than being re-evaluated fresh every tick
    Entity* attackTarget;

    // fired once from Mob_onTick right before a dead mob using this Ai gets
    // removed, matches AI's own beforeRemove() override point, for example
    // Creeper's explosion
    void (*onDeathTimeout)(Ai* ai, Entity* mob);
    // fired once per tick, after the normal wander and attack update, for
    // behavior that runs alongside melee rather than replacing it, matches
    // Skeleton's own tick() override (a chance per tick to shoot an arrow
    // while a target is locked, on top of everything BasicAttackAI already
    // does). NULL for anything with no such extra behavior
    void (*onAfterUpdate)(Ai* ai, Entity* mob);
    // fired once, only after a melee attack actually lands, matches
    // Creeper's own attack() override calling super.attack() first and then
    // doing its own extra thing. NULL for anything with no extra behavior
    // on a landed hit
    void (*onAfterAttack)(Ai* ai, Entity* mob, Entity* target);

    // c0.27_st: matches BasicAI's own jumpFromGround() seam, added so
    // JumpAttackAI (Spider) can override the plain 0.42f hop with a lunging
    // pounce toward a locked target instead. NULL keeps the original inline
    // behavior (every other mob)
    void (*onJumpFromGround)(Ai* ai, Entity* mob);
};

// zeroes wander/chase state and sets the per species runSpeed/damage
// constants (matches BasicAI/BasicAttackAI's own field defaults, then each
// concrete species' own constructor overrides). Call once after allocating
// an Ai
void Ai_init(Ai* ai, bool chase, int wanderPitch, float runSpeed, int damage);

// per tick AI driver, matches BasicAI.tick(): advances the idle despawn
// timer, the wander or chase behavior, applies the water, lava, and jump
// logic, decays speed and turn, and calls Mob_travel to actually move.
// Freezes to fully still once the mob is dead, matching the real source's
// health<=0 branch. Call this from Mob_onTick when mob->ai is non NULL, not
// directly
void Ai_tick(Ai* ai, Entity* mob);

// matches BasicAttackAI.hurt(): resets the idle despawn timer (matches
// BasicAI's own hurt() override, called unconditionally by both wander only
// and chase capable mobs), and, only when chase is true, unwraps an Arrow's
// real owner via killCredit and locks attackTarget onto the attacker if it
// isn't already the mob's own locked target and isn't the same aiClassTag
// as the mob itself (mobs fighting each other, but not a zombie attacking
// another zombie). Call this from Mob_hurt as its very first action, before
// any of the invulnerability or damage bookkeeping, matching the real
// source's own Mob.hurt() exactly
void Ai_onHurt(Ai* ai, Entity* mob, Entity* attacker, int damage);

#endif
