// entity.c: parent entity with physics and movement

#include "entity.h"
#include "level/tile/tile.h"
#include "audio/sound.h"
#include "mob.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define DEG2RAD(d) ((d) * M_PI / 180.0)

void Entity_init(Entity* e, Level* level) {
    e->level = level;
    // matches real source's own Entity() constructor, which explicitly does
    // setPos(0,0,0) first. Every caller overwrites x/y/z via its own
    // Entity_setPosition right after anyway, but prevX/Y/Z below must start
    // from a defined value of 0, not whatever was already sitting in the
    // caller's struct. That was harmless for static or global entities
    // already zero from static storage, but a real uninitialized read for
    // stack allocated ones such as world gen's own scratch Creature in
    // Minecraft_spawnMob
    e->x = e->y = e->z = 0.0f;
    e->xRotation = e->yRotation = 0.f;
    e->prevXRotation = e->prevYRotation = 0.f;
    e->motionX = e->motionY = e->motionZ = 0.0f;
    e->onGround = false;
    e->horizontalCollision = false;
    e->heightOffset = 0.0f;
    e->boundingBoxWidth  = 0.6f;
    e->boundingBoxHeight = 1.8f;
    e->removed = false;
    e->walkDist = 0.0f;
    e->makeStepSound = true;

    // c0.24_st_03: inert defaults; Mob_init overwrites health/airSupply/isMob
    // for entities that actually use them (Player and every mob)
    e->isMob = false;
    e->health = 0;
    e->lastHealth = 0;
    e->invulnerableTime = 0;
    e->hurtTime = e->hurtDuration = 0;
    e->hurtDir = 0.0f;
    e->deathTime = 0;
    e->attackTime = 0;
    e->airSupply = 0;
    e->yBodyRotation = e->prevYBodyRotation = 0.0f;
    e->fallDistance = 0.0f;
    e->dead = false;
    e->onDeath = NULL;
    e->ai = NULL;
    e->killCredit = NULL;
    e->tickCount = 0;
    e->run = e->oRun = 0.0f;
    e->animStep = e->animStepO = 0.0f;

    e->prevX = e->x;
    e->prevY = e->y;
    e->prevZ = e->z;
}

void Entity_setPosition(Entity* e, float x, float y, float z) {
    e->x = x; e->y = y; e->z = z;
    const float w = e->boundingBoxWidth  * 0.5f;
    const float h = e->boundingBoxHeight * 0.5f;
    e->boundingBox = AABB_create(x - w, y - h, z - w, x + w, y + h, z + w);
}

// c0.0.14a_08: real spawn point (Level.xSpawn/ySpawn/zSpawn/rotSpawn) instead
// of random scatter and fall, searching upward from it for a free spot.
// The decompiled Java loop has no exit once free space is found, which
// would hang the game forever on every respawn, so this uses the evidently
// intended search upward until free, then stop logic instead.
void Entity_resetPosition(Entity* e) {
    float x = e->level->xSpawn + 0.5f;
    float y = (float)e->level->ySpawn;
    float z = e->level->zSpawn + 0.5f;

    Entity_setPosition(e, x, y, z);
    while (1) {
        ArrayList_AABB hits = Level_getCubes(e->level, &e->boundingBox);
        int blocked = hits.size != 0;
        if (hits.aabbs) free(hits.aabbs);
        if (!blocked) break;
        y += 1.0f;
        Entity_setPosition(e, x, y, z);
    }

    e->motionX = e->motionY = e->motionZ = 0.0f;
    e->yRotation = e->level->rotSpawn;
    e->xRotation = 0.0f;
}

void Entity_turn(Entity* e, float dx, float dy) {
    e->yRotation += dx * 0.15f;
    e->xRotation -= dy * 0.15f;
    if (e->xRotation >  90.0f) e->xRotation =  90.0f;
    if (e->xRotation < -90.0f) e->xRotation = -90.0f;
}

void Entity_onTick(Entity* e) {
    e->prevX = e->x;
    e->prevY = e->y;
    e->prevZ = e->z;
    e->prevXRotation = e->xRotation;
    e->prevYRotation = e->yRotation;
}

void Entity_move(Entity* e, float dx, float dy, float dz) {
    const float ox = dx, oy = dy, oz = dz;
    const float startX = e->x, startZ = e->z;

    AABB expanded = AABB_expand(&e->boundingBox, dx, dy, dz);
    ArrayList_AABB hits = Level_getCubes(e->level, &expanded);

    for (int i = 0; i < hits.size; ++i) dy = AABB_clipYCollide(&hits.aabbs[i], &e->boundingBox, dy);
    AABB_move(&e->boundingBox, 0.0f, dy, 0.0f);

    for (int i = 0; i < hits.size; ++i) dx = AABB_clipXCollide(&hits.aabbs[i], &e->boundingBox, dx);
    AABB_move(&e->boundingBox, dx, 0.0f, 0.0f);

    for (int i = 0; i < hits.size; ++i) dz = AABB_clipZCollide(&hits.aabbs[i], &e->boundingBox, dz);
    AABB_move(&e->boundingBox, 0.0f, 0.0f, dz);

    e->horizontalCollision = !(ox == dx && oz == dz);
    e->onGround = (oy != dy) && (oy < 0.0f);

    // fall damage. Accumulates while actually falling, using the requested
    // pre collision oy, matching the real source tracking the fall by motion
    // rather than the collision clipped result. Fires once on the tick
    // landing is detected, and clears whenever not currently falling
    if (oy < 0.0f) {
        e->fallDistance -= oy;
    } else {
        e->fallDistance = 0.0f;
    }
    if (e->onGround) {
        Mob_causeFallDamage(e, e->fallDistance);
        e->fallDistance = 0.0f;
    }

    if (ox != dx) e->motionX = 0.0f;
    if (oy != dy) e->motionY = 0.0f;
    if (oz != dz) e->motionZ = 0.0f;

    e->x = (e->boundingBox.minX + e->boundingBox.maxX) * 0.5f;
    e->y =  e->boundingBox.minY + e->heightOffset;
    e->z = (e->boundingBox.minZ + e->boundingBox.maxZ) * 0.5f;

    // c0.0.23a_01: footstep sounds, matching the real source's Entity.move()
    // tail exactly. walkDist accumulates the actual post collision horizontal
    // displacement, not the requested pre collision delta, scaled by 0.6,
    // and fires once it passes 1.0, reading the tile 0.2 units below the feet
    float ddx = e->x - startX;
    float ddz = e->z - startZ;
    e->walkDist += sqrtf(ddx * ddx + ddz * ddz) * 0.6f;
    if (e->makeStepSound) {
        int tileId = Level_getTile(e->level, (int)e->x, (int)(e->y - 0.2f - e->heightOffset), (int)e->z);
        const Tile* t = (tileId > 0 && tileId < 256) ? gTiles[tileId] : NULL;
        if (e->walkDist > 1.0f && t && t->soundType != SOUND_NONE) {
            e->walkDist -= (float)(int)e->walkDist;
            char name[32];
            snprintf(name, sizeof name, "step.%s", SOUND_TYPES[t->soundType].name);
            Sound_play(name, SoundType_getVolume(t->soundType) * 0.75f, SoundType_getPitch(t->soundType),
                       e->x, e->y, e->z);
        }
    }

    if (hits.aabbs) free(hits.aabbs);
}

void Entity_moveRelative(Entity* e, float x, float z, float speed) {
    float d = sqrtf(x*x + z*z);
    if (d < 0.01f) return;
    if (d < 1.0f) d = 1.0f; // c0.0.14a_08: no longer normalizes magnitude under 1 up to unit length

    float k = speed / d;
    x *= k; z *= k;

    const float s = sinf((float)DEG2RAD(e->yRotation));
    const float c = cosf((float)DEG2RAD(e->yRotation));

    e->motionX += x * c - z * s;
    e->motionZ += z * c + x * s;
}

bool Entity_isLit(const Entity* e) {
    return Level_isLit(e->level, (int)e->x, (int)e->y, (int)e->z);
}

float Entity_getBrightness(const Entity* e) {
    return Level_getBrightness(e->level, (int)e->x, (int)(e->y + e->heightOffset / 2.0f), (int)e->z);
}

bool Entity_isFree(const Entity* e, float dx, float dy, float dz) {
    AABB box = AABB_cloneMove(&e->boundingBox, dx, dy, dz);
    ArrayList_AABB hits = Level_getCubes(e->level, &box);
    bool blocked = hits.size > 0;
    if (hits.aabbs) free(hits.aabbs);
    if (blocked) return false;
    return !Level_containsAnyLiquid(e->level, &box);
}

bool Entity_isInWater(const Entity* e) {
    AABB box = AABB_grow(&e->boundingBox, 0.0f, -0.4f, 0.0f);
    return Level_containsLiquid(e->level, &box, LIQUID_WATER);
}

bool Entity_isUnderWater(const Entity* e) {
    int id = Level_getTile(e->level, (int)e->x, (int)(e->y + 0.12f), (int)e->z);
    if (id <= 0) return false;
    const Tile* t = gTiles[id];
    return t && t->liquidType == LIQUID_WATER;
}

bool Entity_isInLava(const Entity* e) {
    return Level_containsLiquid(e->level, &e->boundingBox, LIQUID_LAVA);
}

void Entity_remove(Entity* e) {
    e->removed = true;
}

void Entity_setSize(Entity* e, float width, float height) {
    e->boundingBoxWidth  = width;
    e->boundingBoxHeight = height;
}
