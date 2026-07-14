// mob_ai.c: wander/chase/attack strategy objects, see mob_ai.h

#include "mob_ai.h"
#include "mob.h"
#include "level/level.h"
#include <stdlib.h>
#include <math.h>

static inline float frand01(void) { return (float)rand() / (float)RAND_MAX; }

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
    ai->onDeathTimeout = NULL;
    ai->onAfterUpdate = NULL;
    ai->onAfterAttack = NULL;
}

// matches BasicAttackAI.attack(Entity): blocked by a solid block in the way
// of line of sight, otherwise a smoother roll than the old flat 1 to 7,
// averaging damage plus 1, using two averaged random rolls times the ai's
// own damage constant. Resets the idle despawn timer on every landed hit,
// and fires onAfterAttack for species specific extra behavior (Creeper's
// own self damage) only once the hit actually lands
static void Ai_tryAttack(Ai* ai, Entity* mob, Entity* target) {
    if (Level_clip(mob->level, mob->x, mob->y, mob->z, target->x, target->y, target->z)) return;
    mob->attackTime = MOB_ATTACK_DURATION;
    ai->attackCooldown = (int)(frand01() * 20.0f) + 10;
    int damage = (int)(((frand01() + frand01()) / 2.0f) * (float)ai->damage + 1.0f);
    Mob_hurt(target, mob, damage);
    ai->noActionTime = 0;
    if (ai->onAfterAttack) ai->onAfterAttack(ai, mob, target);
}

// matches BasicAttackAI.doAttack(): acquires the player as a locked target
// within 16 blocks if nothing is currently locked, drops an already removed
// target immediately, and otherwise only gives up a locked target past 32
// blocks with a 1% chance per tick. While a target is locked, faces it every
// tick (the pitch calculation uses the full 3D distance as its denominator,
// not the horizontal only distance this port's own previous version used,
// bytecode confirmed even though it reads like an unusual choice for a pitch
// angle) and attacks once within 2 blocks and off cooldown
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

    if (distSq > detectRange * detectRange * 2.0f * 2.0f && rand() % 100 == 0) {
        ai->attackTarget = NULL;
    }

    if (ai->attackTarget) {
        float dist3d = sqrtf(dx * dx + dy * dy + dz * dz);
        mob->yRotation = (float)(atan2(dz, dx) * 180.0 / M_PI) - 90.0f;
        mob->xRotation = -(float)(atan2(dy, dist3d) * 180.0 / M_PI);
        if (dist3d < 2.0f && ai->attackCooldown == 0) {
            Ai_tryAttack(ai, mob, ai->attackTarget);
        }
    }
}

// matches BasicAI.update(): the plain random wander step, now scaled by the
// ai's own runSpeed rather than a flat magnitude. A 7% chance per tick of a
// new random strafe or forward speed (both scaled by runSpeed), an
// unconditional 1% chance per tick to jump, a 4% chance per tick of a new
// random turn rate of up to 30 degrees either way over the following 60
// ticks. While a target is locked, forward speed is forced to full runSpeed
// and the jump chance rises to 4% per tick (letting a chasing mob jump over
// obstacles more readily), then water or lava overrides the jump chance to
// 80% either way as before
static void Ai_wanderStep(Ai* ai, Entity* mob) {
    if (frand01() < 0.07f) {
        ai->speedX = (frand01() - 0.5f) * ai->runSpeed;
        ai->speedZ = frand01() * ai->runSpeed;
    }
    ai->wantsJump = frand01() < 0.01f;
    if (frand01() < 0.04f) {
        ai->turnSpeed = (frand01() - 0.5f) * 60.0f;
    }
    mob->yRotation += ai->turnSpeed;
    mob->xRotation = (float)ai->wanderPitch;

    if (ai->attackTarget) {
        ai->speedZ = ai->runSpeed;
        ai->wantsJump = frand01() < 0.04f;
    }

    if (Entity_isInWater(mob) || Entity_isInLava(mob)) {
        ai->wantsJump = frand01() < 0.8f;
    }
}

void Ai_tick(Ai* ai, Entity* mob) {
    // matches BasicAI.tick()'s own new idle despawn check: past 600 idle
    // ticks, a 1 in 800 chance per tick checks the 3D distance to the
    // player. Within 32 blocks, the timer just resets (still worth keeping
    // around); past 32 blocks, the mob is removed outright
    ai->noActionTime++;
    if (ai->noActionTime > 600 && rand() % 800 == 0) {
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
    } else {
        Ai_wanderStep(ai, mob);
        if (ai->chase) {
            Ai_doAttack(ai, mob);
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

    // matches Skeleton's own tick() override calling super.tick() first,
    // then its own extra behavior; every other species has no such override
    // so this is a no op for them
    if (ai->onAfterUpdate) ai->onAfterUpdate(ai, mob);
}

void Ai_onHurt(Ai* ai, Entity* mob, Entity* attacker, int damage) {
    (void)damage;
    // matches BasicAI's own hurt() override, called unconditionally for
    // every kind of Ai (wander only or chase capable alike)
    ai->noActionTime = 0;
    if (!ai->chase) return;

    // matches BasicAttackAI's own hurt() override: unwrap an Arrow to its
    // real owner first (killCredit already holds this for Arrow entities,
    // see Creature_onDeath's own identical unwrap), then lock onto whoever
    // or whatever just hurt this mob, as long as it isn't already the
    // locked target and isn't the same kind of thing as this mob itself,
    // which is what lets mobs fight each other without a zombie ever
    // aggroing on another zombie
    Entity* real = attacker;
    if (real && real->killCredit) real = real->killCredit;

    if (real && real->aiClassTag != mob->aiClassTag) {
        ai->attackTarget = real;
    }
}
