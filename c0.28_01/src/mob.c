// mob.c: health, damage, knockback and death

#include "mob.h"
#include "mob_ai.h"
#include "jrandom.h"
#include <math.h>
#include <stdlib.h>

void Mob_init(Entity* e) {
    e->footSize = 0.5f;
    e->rotA = (float)(JRandom_mathRandom() + 1.0) * 0.01f;
    e->timeOffs = (float)JRandom_mathRandom() * 12398.0f;
    e->mobRot = (float)(JRandom_mathRandom() * M_PI * 2.0);
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

void Mob_heal(Entity* e, int amount) {
    if (e->health <= 0) return;
    e->health += amount;
    if (e->health > MOB_MAX_HEALTH) e->health = MOB_MAX_HEALTH;
    e->invulnerableTime = MOB_INVULNERABLE_DURATION / 2;
}

void Mob_hurt(Entity* e, Entity* attacker, int damage) {
    if (!e->isMob || e->health <= 0) return;

    // the ai hears about the hit first
    if (e->ai) Ai_onHurt(e->ai, e, attacker, damage);

    // inside the invulnerability window only the worst hit counts
    if (e->invulnerableTime > MOB_INVULNERABLE_DURATION / 2) {
        if (e->lastHealth - damage >= e->health) return;
        e->health = e->lastHealth - damage;
    } else {
        e->lastHealth = e->health;
        e->invulnerableTime = MOB_INVULNERABLE_DURATION;
        e->hurtTime = e->hurtDuration = MOB_HURT_DURATION;
        e->health -= damage;
    }
    // health can go below 0

    e->hurtDir = 0.0f;
    if (attacker) {
        float dx = attacker->x - e->x;
        float dz = attacker->z - e->z;
        e->hurtDir = (float)(atan2(dz, dx) * 180.0 / M_PI) - e->yRotation;
        Mob_knockback(e, attacker);
    } else {
        e->hurtDir = (float)((int)(JRandom_mathRandom() * 2.0) * 180);
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
        e->motionY = (float)((double)e->motionY - 0.02);
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
        e->motionY = (float)((double)e->motionY - 0.02);
        if (e->horizontalCollision && Entity_isFree(e, e->motionX, e->motionY + 0.6f - e->y + yo, e->motionZ)) {
            e->motionY = 0.3f;
        }
    } else {
        Entity_moveRelative(e, strafe, forward, e->onGround ? 0.1f : 0.02f);
        Entity_move(e, e->motionX, e->motionY, e->motionZ);
        e->motionX *= 0.91f;
        e->motionY *= 0.98f;
        e->motionZ *= 0.91f;
        e->motionY = (float)((double)e->motionY - 0.08);

        if (e->onGround) {
            e->motionX *= 0.6f;
            e->motionZ *= 0.6f;
        }
    }
}

// Mob.tick, a dead mob keeps running all of it, only remove ends it
void Mob_onTick(Entity* e) {
    if (!e->isMob) return;

    if (e->attackTime > 0) e->attackTime--;
    if (e->hurtTime > 0) e->hurtTime--;
    if (e->invulnerableTime > 0) e->invulnerableTime--;
    if (e->health <= 0) {
        e->deathTime++;
        if (e->deathTime > MOB_DEATH_REMOVE_TICKS) {
            if (e->ai && e->ai->onDeathTimeout) e->ai->onDeathTimeout(e->ai, e);
            // removing the player does nothing
            if (e->aiClassTag != AI_CLASS_PLAYER) Entity_remove(e);
        }
    }
    if (Entity_isUnderWater(e)) {
        if (e->airSupply > 0) e->airSupply--;
        else Mob_hurt(e, NULL, 2);
    } else {
        e->airSupply = MOB_TOTAL_AIR_SUPPLY;
    }
    if (Entity_isInWater(e)) e->fallDistance = 0.0f;
    if (Entity_isInLava(e)) Mob_hurt(e, NULL, 10);

    e->animStepO = e->animStep;
    e->prevYBodyRotation = e->yBodyRotation;
    e->tickCount++;
    if (e->ai) Ai_tick(e->ai, e);

    float dx = e->x - e->prevX;
    float dz = e->z - e->prevZ;
    float moveDist = (float)sqrt(dx * dx + dz * dz);
    float facing = e->yBodyRotation;
    float animDelta = 0.0f;
    e->oRun = e->run;
    float targetRun = 0.0f;
    if (!(moveDist <= 0.05f)) {
        targetRun = 1.0f;
        animDelta = moveDist * 3.0f;
        facing = (float)atan2(dz, dx) * 180.0f / (float)M_PI - 90.0f;
    }
    if (!e->onGround) targetRun = 0.0f;
    e->run += (targetRun - e->run) * 0.3f;

    float towardFacing = facing - e->yBodyRotation;
    while (towardFacing < -180.0f) towardFacing += 360.0f;
    while (towardFacing >= 180.0f) towardFacing -= 360.0f;
    e->yBodyRotation += towardFacing * 0.1f;

    float headOffset = e->yRotation - e->yBodyRotation;
    while (headOffset < -180.0f) headOffset += 360.0f;
    while (headOffset >= 180.0f) headOffset -= 360.0f;
    bool facingAway = headOffset < -90.0f || headOffset >= 90.0f;
    if (headOffset < -75.0f) headOffset = -75.0f;
    if (headOffset >= 75.0f) headOffset = 75.0f;
    e->yBodyRotation = e->yRotation - headOffset;
    e->yBodyRotation += headOffset * 0.1f;
    if (facingAway) animDelta = -animDelta;

    // keep the previous angles on the near side so interpolation never spins the long way
    while (e->yRotation - e->prevYRotation < -180.0f) e->prevYRotation -= 360.0f;
    while (e->yRotation - e->prevYRotation >= 180.0f) e->prevYRotation += 360.0f;
    while (e->yBodyRotation - e->prevYBodyRotation < -180.0f) e->prevYBodyRotation -= 360.0f;
    while (e->yBodyRotation - e->prevYBodyRotation >= 180.0f) e->prevYBodyRotation += 360.0f;
    while (e->xRotation - e->prevXRotation < -180.0f) e->prevXRotation -= 360.0f;
    while (e->xRotation - e->prevXRotation >= 180.0f) e->prevXRotation += 360.0f;

    e->animStep += animDelta;
}
