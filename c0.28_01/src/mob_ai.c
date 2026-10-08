// mob_ai.c

#include "mob_ai.h"
#include "math_helper.h"
#include "mob.h"
#include "level/level.h"
#include <stdlib.h>
#include <math.h>

extern void Minecraft_pushNearby(Entity* mob);

void Ai_init(Ai* ai, bool chase, int wanderPitch, float runSpeed, int damage) {
    ai->chase = chase;
    ai->wanderPitch = wanderPitch;
    ai->runSpeed = runSpeed;
    ai->damage = damage;
    ai->speedX = ai->speedZ = 0.0f;
    ai->turnSpeed = 0.0f;
    ai->wantsJump = false;
    ai->attackCooldown = 0;
    ai->noActionTime = 0;
    ai->attackTarget = NULL;
    ai->jumpAttack = false;
    JRandom_initRandom(&ai->random);
    ai->onDeathTimeout = NULL;
    ai->onAfterUpdate = NULL;
    ai->onUpdate = NULL;
    ai->onAfterAttack = NULL;
}

// no hit through a solid block, damage is (rand+rand)/2*damage+1
static void Ai_tryAttack(Ai* ai, Entity* mob, Entity* target) {
    if (Level_clip(mob->level, mob->x, mob->y, mob->z, target->x, target->y, target->z)) return;
    mob->attackTime = MOB_ATTACK_DURATION;
    ai->attackCooldown = JRandom_nextIntN(&ai->random, 20) + 10;
    int damage = (int)(((JRandom_nextFloat(&ai->random) + JRandom_nextFloat(&ai->random)) / 2.0f) * (float)ai->damage + 1.0f);
    Mob_hurt(target, mob, damage);
    ai->noActionTime = 0;
    if (ai->onAfterAttack) ai->onAfterAttack(ai, mob, target);
}

// drops the target past 32 blocks 1 in 100 a tick
// the pitch uses the full 3d distance
static void Ai_doAttack(Ai* ai, Entity* mob) {
    const float detectRange = 16.0f;

    if (ai->attackTarget && ai->attackTarget->removed) {
        ai->attackTarget = NULL;
    }

    if (!ai->attackTarget) {
        Entity* player = Level_getPlayer(mob->level);
        if (player) {
            float dx = player->x - mob->x;
            float dy = player->y - mob->y;
            float dz = player->z - mob->z;
            float distSq = dx * dx + dy * dy + dz * dz;
            if (distSq < detectRange * detectRange) ai->attackTarget = player;
        }
    }

    if (!ai->attackTarget) return;

    float dx = ai->attackTarget->x - mob->x;
    float dy = ai->attackTarget->y - mob->y;
    float dz = ai->attackTarget->z - mob->z;
    float distSq = dx * dx + dy * dy + dz * dz;

    if (distSq > detectRange * detectRange * 2.0f * 2.0f && JRandom_nextIntN(&ai->random, 100) == 0) {
        ai->attackTarget = NULL;
    }

    if (ai->attackTarget) {
        float dist3d = MathHelper_sqrt(dx * dx + dy * dy + dz * dz);
        mob->yRotation = (float)(atan2(dz, dx) * 180.0 / 3.1415927410125732) - 90.0f;
        mob->xRotation = -(float)(atan2(dy, dist3d) * 180.0 / 3.1415927410125732);
        if (dist3d < 2.0f && ai->attackCooldown == 0) {
            Ai_tryAttack(ai, mob, ai->attackTarget);
        }
    }
}

static void Ai_wanderStep(Ai* ai, Entity* mob) {
    if (JRandom_nextFloat(&ai->random) < 0.07f) {
        ai->speedX = (JRandom_nextFloat(&ai->random) - 0.5f) * ai->runSpeed;
        ai->speedZ = JRandom_nextFloat(&ai->random) * ai->runSpeed;
    }
    ai->wantsJump = JRandom_nextFloat(&ai->random) < 0.01f;
    if (JRandom_nextFloat(&ai->random) < 0.04f) {
        ai->turnSpeed = (JRandom_nextFloat(&ai->random) - 0.5f) * 60.0f;
    }
    mob->yRotation += ai->turnSpeed;
    mob->xRotation = (float)ai->wanderPitch;

    if (ai->attackTarget) {
        ai->speedZ = ai->runSpeed;
        ai->wantsJump = JRandom_nextFloat(&ai->random) < 0.04f;
    }

    if (Entity_isInWater(mob) || Entity_isInLava(mob)) {
        ai->wantsJump = JRandom_nextFloat(&ai->random) < 0.8f;
    }
}

void Ai_update(Ai* ai, Entity* mob) {
    Ai_wanderStep(ai, mob);
    if (ai->chase) Ai_doAttack(ai, mob);
}

void Ai_tick(Ai* ai, Entity* mob) {
    // idle 600 ticks, then 1 in 800 a tick it gets removed if over 32 blocks from the player
    ai->noActionTime++;
    if (ai->noActionTime > 600 && JRandom_nextIntN(&ai->random, 800) == 0) {
        Entity* player = Level_getPlayer(mob->level);
        if (player) {
            float dx = player->x - mob->x;
            float dy = player->y - mob->y;
            float dz = player->z - mob->z;
            float distSq = dx * dx + dy * dy + dz * dz;
            if (distSq < 1024.0f) {
                ai->noActionTime = 0;
            } else {
                Entity_remove(mob);
            }
        }
    }

    if (ai->attackCooldown > 0) ai->attackCooldown--;

    if (mob->health <= 0) {
        ai->wantsJump = false;
        ai->speedX = ai->speedZ = 0.0f;
        ai->turnSpeed = 0.0f;
    } else if (ai->onUpdate) {
        ai->onUpdate(ai, mob);
    } else {
        Ai_update(ai, mob);
    }

    bool inWater = Entity_isInWater(mob);
    bool inLava  = Entity_isInLava(mob);
    if (ai->wantsJump) {
        if (inWater)            mob->motionY += 0.04f;
        else if (inLava)        mob->motionY += 0.04f;
        else if (mob->onGround) {
            // jumpFromGround
            if (ai->jumpAttack && ai->attackTarget) {
                mob->motionX = 0.0f;
                mob->motionZ = 0.0f;
                Entity_moveRelative(mob, 0.0f, 1.0f, 0.6f);
                mob->motionY = 0.5f;
            } else {
                mob->motionY = 0.42f;
            }
        }
    }

    ai->speedX *= 0.98f;
    ai->speedZ *= 0.98f;
    ai->turnSpeed *= 0.9f;

    Mob_travel(mob, ai->speedX, ai->speedZ);

    Minecraft_pushNearby(mob);

    if (ai->onAfterUpdate) ai->onAfterUpdate(ai, mob);
}

void Ai_onHurt(Ai* ai, Entity* mob, Entity* attacker, int damage) {
    (void)damage;
    ai->noActionTime = 0;
    if (!ai->chase) return;

    // an arrow counts as its shooter
    Entity* real = attacker;
    if (real && real->killCredit) real = real->killCredit;

    if (real && real->aiClassTag != mob->aiClassTag) {
        ai->attackTarget = real;
    }
}
