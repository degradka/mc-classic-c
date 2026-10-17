// mob_ai.h: BasicAI and BasicAttackAI in one struct, chase picks the attack one

#ifndef MOB_AI_H
#define MOB_AI_H

#include "entity.h"
#include "jrandom.h"
#include <stdbool.h>

struct Ai {
    // defaultLookAngle, 0 pig, 30 zombie and skeleton, 45 creeper
    int wanderPitch;
    // BasicAttackAI when true, plain BasicAI wandering when false
    bool chase;

    // 0.7 by default, 1.0 zombie, 0.3 skeleton
    float runSpeed;
    // 6 by default, 8 skeleton
    int damage;

    // BasicAI fields
    float speedX, speedZ;  // xxa, yya: current strafe/forward travel speed
    float turnSpeed;        // yRotA: current yaw drift per tick
    bool  wantsJump;         // jumping
    int   attackCooldown;    // attackDelay: ticks left before another melee attempt
    int   noActionTime;
    // BasicAI.random, one per mob
    JRandom random;
    bool jumpAttack;        // JumpAttackAI, the spider leaps at its target
    Entity* attackTarget;

    // beforeRemove, the creeper's explosion
    void (*onDeathTimeout)(Ai* ai, Entity* mob);
    // the skeleton's shooting, on top of the melee
    void (*onAfterUpdate)(Ai* ai, Entity* mob);
    // only after a hit lands, the creeper's self damage
    void (*onAfterAttack)(Ai* ai, Entity* mob, Entity* target);
    // replaces BasicAI.update, the sheep's grazing, which calls Ai_update itself
    void (*onUpdate)(Ai* ai, Entity* mob);
};

void Ai_init(Ai* ai, bool chase, int wanderPitch, float runSpeed, int damage);

// BasicAI.tick, called from Mob_onTick
void Ai_tick(Ai* ai, Entity* mob);
// BasicAI.update, or BasicAttackAI.update when chase is on
void Ai_update(Ai* ai, Entity* mob);

// BasicAttackAI.hurt, Mob_hurt calls it first
void Ai_onHurt(Ai* ai, Entity* mob, Entity* attacker, int damage);

#endif
