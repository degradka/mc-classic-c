// mob_ai.h: BasicAI and BasicAttackAI in one struct with a chase flag

#ifndef MOB_AI_H
#define MOB_AI_H

#include "entity.h"
#include <stdbool.h>

struct Ai {
    int wanderPitch;
    bool chase;

    float speedX, speedZ;
    float turnSpeed;
    bool  wantsJump;        // jumping
    int   attackCooldown;

    // AI.beforeRemove, creeper explodes
    void (*onDeathTimeout)(Ai* ai, Entity* mob);
};

void Ai_init(Ai* ai, bool chase, int wanderPitch);

void Ai_tick(Ai* ai, Entity* mob);

#endif
