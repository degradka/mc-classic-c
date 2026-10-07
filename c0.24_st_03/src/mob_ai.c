// mob_ai.c: BasicAI.tick and update, plus the attacking subclass, see mob_ai.h

#include "mob_ai.h"
#include "mob.h"
#include "level/level.h"
#include <stdlib.h>
#include <math.h>

extern void Minecraft_pushNearby(Entity* mob);

void Ai_init(Ai* ai, bool chase, int wanderPitch) {
    ai->chase = chase;
    ai->wanderPitch = wanderPitch;
    ai->speedX = ai->speedZ = 0.0f;
    ai->turnSpeed = 0.0f;
    ai->wantsJump = false;
    ai->attackCooldown = 0;
    JRandom_initRandom(&ai->random);
    ai->onDeathTimeout = NULL;
    ai->onAfterAttack = NULL;
}

// no hit through anything in the line of sight
static void attack(Ai* ai, Entity* mob, Entity* target) {
    if (!Level_clip(mob->level, mob->x, mob->y, mob->z, target->x, target->y, target->z)) {
        mob->attackTime = MOB_ATTACK_DURATION;
        ai->attackCooldown = JRandom_nextIntN(&ai->random, 20) + 10;
        int a = JRandom_nextIntN(&ai->random, 4);
        int b = JRandom_nextIntN(&ai->random, 4);
        Mob_hurt(target, mob, a + b + 1);
    }
    if (ai->onAfterAttack) ai->onAfterAttack(ai, mob, target);
}

// the pitch uses the flat distance
static void lookAndAttack(Ai* ai, Entity* mob, Entity* target) {
    if (!target) return;
    float dx = target->x - mob->x;
    float dz = target->z - mob->z;
    float flat = (float)sqrt(dx * dx + dz * dz);
    if (flat < 8.0f) {
        float dy = target->y - mob->y;
        mob->yRotation = (float)(atan2(dz, dx) * 180.0 / M_PI) - 90.0f;
        mob->xRotation = -((float)(atan2(dy, flat) * 180.0 / M_PI));
        float dist = (float)sqrt(dx * dx + dy * dy + dz * dz);
        if (dist < 2.0f && ai->attackCooldown == 0) attack(ai, mob, target);
    }
}

// BasicAI.update
static void update(Ai* ai, Entity* mob) {
    if (JRandom_nextFloat(&ai->random) < 0.07f) {
        ai->speedX = JRandom_nextFloat(&ai->random) - 0.5f;
        ai->speedZ = JRandom_nextFloat(&ai->random);
    }
    if (JRandom_nextFloat(&ai->random) < 0.04f) {
        ai->turnSpeed = (JRandom_nextFloat(&ai->random) - 0.5f) * 60.0f;
    }
    mob->yRotation += ai->turnSpeed;
    mob->xRotation = (float)ai->wanderPitch;
    ai->wantsJump = JRandom_nextFloat(&ai->random) < 0.01f;
    if (Entity_isInWater(mob) || Entity_isInLava(mob)) {
        ai->wantsJump = JRandom_nextFloat(&ai->random) < 0.8f;
    }
    if (ai->chase) lookAndAttack(ai, mob, Level_getPlayer(mob->level));
}

void Ai_tick(Ai* ai, Entity* mob) {
    if (ai->attackCooldown > 0) ai->attackCooldown--;

    if (mob->health <= 0) {
        ai->wantsJump = false;
        ai->speedX = ai->speedZ = 0.0f;
        ai->turnSpeed = 0.0f;
    } else {
        update(ai, mob);
    }

    bool inWater = Entity_isInWater(mob);
    bool inLava  = Entity_isInLava(mob);
    if (ai->wantsJump) {
        if (inWater)            mob->motionY += 0.04f;
        else if (inLava)        mob->motionY += 0.04f;
        else if (mob->onGround) mob->motionY = 0.42f;
    }

    ai->speedX *= 0.98f;
    ai->speedZ *= 0.98f;
    ai->turnSpeed *= 0.9f;

    Mob_travel(mob, ai->speedX, ai->speedZ);

    Minecraft_pushNearby(mob);
}
