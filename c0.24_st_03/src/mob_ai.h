// mob_ai.h: BasicAI and BasicAttackAI in one struct with a chase flag

#ifndef MOB_AI_H
#define MOB_AI_H

#include "entity.h"
#include "jrandom.h"
#include <stdbool.h>

struct Ai {
    int wanderPitch;        // AI.defaultLookAngle
    bool chase;             // the attacking subclass, looks at and hits the player
    float speedX, speedZ;
    float turnSpeed;
    bool  wantsJump;        // jumping
    int   attackCooldown;
    JRandom random;
    // AI.beforeRemove, creeper explodes
    void (*onDeathTimeout)(Ai* ai, Entity* mob);
    // runs after every attack attempt, creeper hurts itself
    void (*onAfterAttack)(Ai* ai, Entity* mob, Entity* target);
};

void Ai_init(Ai* ai, bool chase, int wanderPitch);
void Ai_tick(Ai* ai, Entity* mob);

#endif
