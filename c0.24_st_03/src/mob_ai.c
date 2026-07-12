// mob_ai.c: wander/chase/attack strategy objects, see mob_ai.h

#include "mob_ai.h"
#include "mob.h"
#include "level/level.h"
#include <stdlib.h>
#include <math.h>

static inline float frand01(void) { return (float)rand() / (float)RAND_MAX; }

void Ai_init(Ai* ai, bool chase, int wanderPitch) {
    ai->chase = chase;
    ai->wanderPitch = wanderPitch;
    ai->speedX = ai->speedZ = 0.0f;
    ai->turnSpeed = 0.0f;
    ai->wantsJump = false;
    ai->attackCooldown = 0;
    ai->onDeathTimeout = NULL;
}

// matches class c's b(Entity) attack attempt: blocked by a solid block in
// the way of line of sight, otherwise flat damage from 1 to 7, two
// overlapping rolls of 0 to 3 plus 1, and a cooldown of 10 to 30 ticks
// before the next attempt
static void Ai_tryAttack(Ai* ai, Entity* mob, Entity* target) {
    if (Level_clip(mob->level, mob->x, mob->y, mob->z, target->x, target->y, target->z)) return;
    mob->attackTime = MOB_ATTACK_DURATION;
    ai->attackCooldown = (int)(frand01() * 20.0f) + 10;
    Mob_hurt(target, mob, (int)(frand01() * 4.0f) + (int)(frand01() * 4.0f) + 1);
}

// matches class c's a(Entity): within 8 blocks, turn to face the target,
// which combined with the wander step's already forward speedZ is what
// actually walks the mob toward it, there is no separate move toward step.
// Within 2 blocks, measured in 3D after the turn, and off cooldown, attack
static void Ai_seekAndAttack(Ai* ai, Entity* mob, Entity* target) {
    if (!target) return;

    float dx = target->x - mob->x;
    float dz = target->z - mob->z;
    float distXZ = sqrtf(dx * dx + dz * dz);
    if (distXZ >= 8.0f) return;

    float dy = target->y - mob->y;
    mob->yRotation = (float)(atan2(dz, dx) * 180.0 / M_PI) - 90.0f;
    mob->xRotation = -(float)(atan2(dy, distXZ) * 180.0 / M_PI);

    float dist3d = sqrtf(dx * dx + dy * dy + dz * dz);
    if (dist3d < 2.0f && ai->attackCooldown == 0) {
        Ai_tryAttack(ai, mob, target);
    }
}

// matches class c's b(): the plain random wander step. A 7% chance per tick
// of a new random strafe or forward speed, a 4% chance per tick of a new
// random turn rate of up to 30 degrees either way over the following 60
// ticks, and a 1% chance per tick to jump, 80% instead while in water or
// lava
static void Ai_wanderStep(Ai* ai, Entity* mob) {
    if (frand01() < 0.07f) {
        ai->speedX = frand01() - 0.5f;
        ai->speedZ = frand01();
    }
    if (frand01() < 0.04f) {
        ai->turnSpeed = (frand01() - 0.5f) * 60.0f;
    }
    mob->yRotation += ai->turnSpeed;
    mob->xRotation = (float)ai->wanderPitch;

    ai->wantsJump = frand01() < 0.01f;
    if (Entity_isInWater(mob) || Entity_isInLava(mob)) {
        ai->wantsJump = frand01() < 0.8f;
    }
}

void Ai_tick(Ai* ai, Entity* mob) {
    if (ai->attackCooldown > 0) ai->attackCooldown--;

    if (mob->health <= 0) {
        ai->wantsJump = false;
        ai->speedX = ai->speedZ = 0.0f;
        ai->turnSpeed = 0.0f;
    } else {
        Ai_wanderStep(ai, mob);
        if (ai->chase) {
            Ai_seekAndAttack(ai, mob, Level_getPlayer(mob->level));
        }
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

    // real source's tail end also pushes apart any nearby entities
    // overlapping the mob's slightly grown bounding box every AI tick, via
    // Level.findEntities plus Entity.isPushable/push. Deliberately not
    // ported yet: it needs a query that finds nearby entities across every
    // type at once, which this codebase doesn't have since entities live in
    // separate per type arrays, not one shared list like the real
    // Level.entities. Purely cosmetic, stops mobs standing exactly on top
    // of each other
}
