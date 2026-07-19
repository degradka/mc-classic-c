// mob.c: shared health/damage/knockback/invulnerability state and behavior

#include "mob.h"
#include "mob_ai.h"
#include <math.h>
#include <stdlib.h>

void Mob_init(Entity* e) {
    e->isMob = true;
    e->health = MOB_MAX_HEALTH;
    e->lastHealth = MOB_MAX_HEALTH;
    e->airSupply = MOB_TOTAL_AIR_SUPPLY;
    e->invulnerableTime = 0;
    e->hurtTime = e->hurtDuration = 0;
    e->hurtDir = 0.0f;
    e->deathTime = 0;
    e->attackTime = 0;
    e->dead = false;
    // c0.27_st: new auto-step mechanic, matches Mob's own constructor
    // setting footSize=0.5 for every Mob (and Player, which also routes
    // through here), letting them climb a half block ledge without jumping
    e->footSize = 0.5f;
}

void Mob_knockback(Entity* e, Entity* attacker) {
    float dx = e->x - attacker->x;
    float dz = e->z - attacker->z;
    float dist = sqrtf(dx * dx + dz * dz);

    e->motionX *= 0.5f;
    e->motionY *= 0.5f;
    e->motionZ *= 0.5f;

    if (dist > 0.0001f) {
        e->motionX += (dx / dist) * 0.4f;
        e->motionZ += (dz / dist) * 0.4f;
    }
    e->motionY += 0.4f;
    if (e->motionY > 0.4f) e->motionY = 0.4f;
}

void Mob_hurt(Entity* e, Entity* attacker, int damage) {
    if (!e->isMob || e->health <= 0) return;

    // matches Mob.hurt()'s own very first action, before any invulnerability
    // or damage bookkeeping: lets the AI reset its idle timer and, for chase
    // capable mobs, lock onto whoever or whatever just landed this hit
    if (e->ai) Ai_onHurt(e->ai, e, attacker, damage);

    // still in the strong half of the invulnerability window: this hit only
    // actually lands if applying it fresh from lastHealth (the health value
    // from before this window started) would be worse than whatever's
    // already been applied this window, i.e. only the single worst hit in
    // a window wins, rather than every simultaneous hit stacking additively
    if (e->invulnerableTime > MOB_INVULNERABLE_DURATION / 2) {
        if (e->lastHealth - damage >= e->health) return;
        e->health = e->lastHealth - damage;
    } else {
        e->lastHealth = e->health;
        e->invulnerableTime = MOB_INVULNERABLE_DURATION;
        e->hurtTime = e->hurtDuration = MOB_HURT_DURATION;
        e->health -= damage;
    }
    // no floor at 0 here, matching the real source exactly: health can go
    // negative, harmless since only <=0 is ever checked against it

    // hurt flash wobble direction: angle toward the attacker relative to
    // this mob's own facing, or a random left or right flip for
    // environmental damage when attacker is NULL. Bytecode verified against
    // Mob.hurt()
    e->hurtDir = 0.0f;
    if (attacker) {
        float dx = attacker->x - e->x;
        float dz = attacker->z - e->z;
        e->hurtDir = (float)(atan2(dz, dx) * 180.0 / M_PI) - e->yRotation;
        Mob_knockback(e, attacker);
    } else {
        e->hurtDir = (float)((int)(((float)rand() / RAND_MAX) * 2.0f)) * 180.0f;
    }

    if (e->health <= 0 && !e->dead) {
        e->dead = true;
        if (e->onDeath) e->onDeath(e, attacker);
    }
}

void Mob_causeFallDamage(Entity* e, float fallDistance) {
    if (!e->isMob) return;
    int damage = (int)ceilf(fallDistance - 3.0f);
    if (damage > 0) Mob_hurt(e, NULL, damage);
}

void Mob_heal(Entity* e, int amount) {
    if (e->health <= 0) return;
    e->health += amount;
    if (e->health > MOB_MAX_HEALTH) e->health = MOB_MAX_HEALTH;
    e->invulnerableTime = MOB_INVULNERABLE_DURATION / 2;
}

void Mob_travel(Entity* e, float strafe, float forward) {
    bool inWater = Entity_isInWater(e);
    bool inLava  = Entity_isInLava(e);

    if (inWater) {
        float yo = e->y;
        Entity_moveRelative(e, strafe, forward, 0.02f);
        Entity_move(e, e->motionX, e->motionY, e->motionZ);
        e->motionX *= 0.8f;
        e->motionY *= 0.8f;
        e->motionZ *= 0.8f;
        e->motionY -= 0.02f;
        if (e->horizontalCollision && Entity_isFree(e, e->motionX, e->motionY + 0.6f - e->y + yo, e->motionZ)) {
            e->motionY = 0.3f;
        }
    } else if (inLava) {
        float yo = e->y;
        Entity_moveRelative(e, strafe, forward, 0.02f);
        Entity_move(e, e->motionX, e->motionY, e->motionZ);
        e->motionX *= 0.5f;
        e->motionY *= 0.5f;
        e->motionZ *= 0.5f;
        e->motionY -= 0.02f;
        if (e->horizontalCollision && Entity_isFree(e, e->motionX, e->motionY + 0.6f - e->y + yo, e->motionZ)) {
            e->motionY = 0.3f;
        }
    } else {
        Entity_moveRelative(e, strafe, forward, e->onGround ? 0.1f : 0.02f);
        Entity_move(e, e->motionX, e->motionY, e->motionZ);
        e->motionX *= 0.91f;
        e->motionY *= 0.98f;
        e->motionZ *= 0.91f;
        e->motionY -= 0.08f;

        if (e->onGround) {
            e->motionX *= 0.6f;
            e->motionZ *= 0.6f;
        }
    }
}

void Mob_onTick(Entity* e) {
    if (!e->isMob) return;

    if (e->invulnerableTime > 0) e->invulnerableTime--;
    if (e->hurtTime > 0) e->hurtTime--;
    if (e->attackTime > 0) e->attackTime--;

    if (e->dead) {
        e->deathTime++;
        if (e->deathTime > MOB_DEATH_REMOVE_TICKS) {
            if (e->ai && e->ai->onDeathTimeout) e->ai->onDeathTimeout(e->ai, e);
            Entity_remove(e);
            return;
        }
    } else {
        // drowning: 2 damage per tick once air runs out. Matches
        // Mob.java:89-97 exactly, gated on isUnderWater(), the single tile
        // at eye level, x/y+0.12/z, must actually be water, not the broader
        // isInWater() torso grown overlap test. Standing in ankle or waist
        // deep water with eyes still above the surface must not consume
        // air at all
        if (Entity_isUnderWater(e)) {
            if (e->airSupply > 0) e->airSupply--;
            else Mob_hurt(e, NULL, 2);
        } else {
            e->airSupply = MOB_TOTAL_AIR_SUPPLY;
        }

        if (Entity_isInLava(e)) {
            Mob_hurt(e, NULL, 10);
        }

        // water breaks a fall, matches Mob.java:98-100 exactly:
        // isInWater() sets fallDistance to 0. Without this, sinking down
        // through a deep water column onto a submerged floor kept
        // accumulating fallDistance the whole way down and could trigger
        // fall damage on landing even though the actual descent was always
        // at water's slow sink speed, never a real fall
        if (Entity_isInWater(e)) {
            e->fallDistance = 0.0f;
        }
    }

    // AI driven movement, wander, chase and attack, matches Mob.tick()'s own
    // aiStep() call, which the real source runs unconditionally, dead or
    // not. Ai_tick's own health<=0 branch freezes the wander and turn
    // inputs but still calls Mob_travel, so a mob's corpse keeps falling
    // and settling under its existing momentum instead of freezing in mid
    // air the instant it dies. NULL for Player, whose movement stays key
    // driven; every other Mob owns one of these
    if (e->ai) Ai_tick(e->ai, e);

    if (e->dead) return; // walk cycle below is a live-only cosmetic effect

    // walk cycle drivers for this Mob's own rendered model, matches
    // Mob.tick()'s own tail exactly, bytecode verified. run/oRun blends
    // toward 1 while actually moving on the ground, animStep accumulates by
    // actual speed times 3 while running, yBodyRotation first eases toward
    // the movement's own facing direction, or just holds still if not
    // moving, then gets reclamped to stay within 75 degrees either way of
    // the live head yaw, matching Ai_doAttack turning the head to face
    // a target while the body catches up. If the head ends up more than 90
    // degrees off body facing, looking backward relative to travel, the leg
    // animation runs in reverse instead of freezing
    e->tickCount++;
    e->animStepO = e->animStep;
    e->prevYBodyRotation = e->yBodyRotation;

    float dx = e->x - e->prevX;
    float dz = e->z - e->prevZ;
    float moveDist = sqrtf(dx * dx + dz * dz);
    float facing = e->yBodyRotation;
    float animDelta = 0.0f;
    float targetRun = 0.0f;
    e->oRun = e->run;
    if (moveDist > 0.05f) {
        targetRun = 1.0f;
        animDelta = moveDist * 3.0f;
        facing = (float)(atan2(dz, dx) * 180.0 / M_PI) - 90.0f;
    }
    if (!e->onGround) targetRun = 0.0f;
    e->run += (targetRun - e->run) * 0.3f;

    float towardFacing = facing - e->yBodyRotation;
    while (towardFacing < -180.0f) towardFacing += 360.0f;
    while (towardFacing >= 180.0f)  towardFacing -= 360.0f;
    e->yBodyRotation += towardFacing * 0.1f;

    float headOffset = e->yRotation - e->yBodyRotation;
    while (headOffset < -180.0f) headOffset += 360.0f;
    while (headOffset >= 180.0f)  headOffset -= 360.0f;
    bool facingAway = (headOffset < -90.0f || headOffset >= 90.0f);
    if (headOffset < -75.0f) headOffset = -75.0f;
    if (headOffset >=  75.0f) headOffset =  75.0f;
    e->yBodyRotation = e->yRotation - headOffset;
    e->yBodyRotation += headOffset * 0.1f;
    if (facingAway) animDelta = -animDelta;

    e->animStep += animDelta;
}
