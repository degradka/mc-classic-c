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
    e->footSize = 0.0f;
    e->ySlideOffset = 0.0f;

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
    e->aiClassTag = AI_CLASS_NONE;
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
    const AABB origBox = e->boundingBox;

    AABB expanded = AABB_expand(&e->boundingBox, dx, dy, dz);
    ArrayList_AABB hits = Level_getCubes(e->level, &expanded);

    for (int i = 0; i < hits.size; ++i) dy = AABB_clipYCollide(&hits.aabbs[i], &e->boundingBox, dy);
    AABB_move(&e->boundingBox, 0.0f, dy, 0.0f);

    // matches real source exactly: "grounded" for the auto-step check below
    // means either already on ground before this move, or just landed via
    // this move's own Y clip
    bool wasGrounded = e->onGround || (oy != dy && oy < 0.0f);

    for (int i = 0; i < hits.size; ++i) dx = AABB_clipXCollide(&hits.aabbs[i], &e->boundingBox, dx);
    AABB_move(&e->boundingBox, dx, 0.0f, 0.0f);

    for (int i = 0; i < hits.size; ++i) dz = AABB_clipZCollide(&hits.aabbs[i], &e->boundingBox, dz);
    AABB_move(&e->boundingBox, 0.0f, 0.0f, dz);

    if (hits.aabbs) free(hits.aabbs);

    // c0.27_st: new auto-step mechanic. If horizontal movement got blocked
    // while grounded, retry the whole move from the pre-collision box with
    // up to footSize of vertical rise allowed instead of the real dy. Only
    // commit to the stepped result if it actually covers more horizontal
    // ground than the original attempt; otherwise revert. Matches real
    // source's own footSize/ySlideOffset mechanic exactly (Player and every
    // Mob get footSize=0.5, see Mob_init); previously entirely missing, so
    // mobs (and the player) could not climb a single half block ledge
    // without jumping
    if (e->footSize > 0.0f && wasGrounded && e->ySlideOffset < 0.05f && (ox != dx || oz != dz)) {
        // CORRECTION: must compare against the already blocked dx/dz (this
        // normal pass's own clipped result), not the original pre-clip
        // ox/oz, since comparing against ox/oz meant a fully successful step
        // (which recovers exactly the original requested distance) always
        // came out "equal, not greater" and never actually committed
        const float blockedDx = dx, blockedDz = dz;

        AABB stepBox = origBox;
        AABB stepExpanded = AABB_expand(&stepBox, ox, e->footSize, oz);
        ArrayList_AABB stepHits = Level_getCubes(e->level, &stepExpanded);

        float sdy = e->footSize, sdx = ox, sdz = oz;
        for (int i = 0; i < stepHits.size; ++i) sdy = AABB_clipYCollide(&stepHits.aabbs[i], &stepBox, sdy);
        AABB_move(&stepBox, 0.0f, sdy, 0.0f);
        for (int i = 0; i < stepHits.size; ++i) sdx = AABB_clipXCollide(&stepHits.aabbs[i], &stepBox, sdx);
        AABB_move(&stepBox, sdx, 0.0f, 0.0f);
        for (int i = 0; i < stepHits.size; ++i) sdz = AABB_clipZCollide(&stepHits.aabbs[i], &stepBox, sdz);
        AABB_move(&stepBox, 0.0f, 0.0f, sdz);

        if (stepHits.aabbs) free(stepHits.aabbs);

        if (blockedDx * blockedDx + blockedDz * blockedDz < sdx * sdx + sdz * sdz) {
            // the stepped attempt covers more ground than the blocked
            // normal attempt did, so keep it
            dx = sdx; dy = sdy; dz = sdz;
            e->boundingBox = stepBox;
            e->ySlideOffset += 0.5f;
        }
        // else: the step didn't help, keep the original (already applied) result
    }

    e->horizontalCollision = !(ox == dx && oz == dz);
    e->onGround = (oy != dy) && (oy < 0.0f);

    // fall damage. CORRECTION: bytecode confirmed real source keys this off
    // the FINAL dy (post collision, post auto step), not the original
    // requested oy, and while airborne with dy>=0 (e.g. a mid air knockback
    // bounce interrupting a fall) fallDistance is left completely untouched
    // rather than reset to 0, since only actually landing (onGround) clears it.
    // The previous version zeroed fallDistance early on any such interruption,
    // understating eventual fall damage
    if (e->onGround) {
        if (e->fallDistance > 0.0f) Mob_causeFallDamage(e, e->fallDistance);
        e->fallDistance = 0.0f;
    } else if (dy < 0.0f) {
        e->fallDistance -= dy;
    }

    if (ox != dx) e->motionX = 0.0f;
    if (oy != dy) e->motionY = 0.0f;
    if (oz != dz) e->motionZ = 0.0f;

    e->x = (e->boundingBox.minX + e->boundingBox.maxX) * 0.5f;
    // c0.27_st: subtracts ySlideOffset, smoothing the auto-step's visual
    // rise over a few ticks instead of snapping straight to the new height
    e->y =  e->boundingBox.minY + e->heightOffset - e->ySlideOffset;
    e->z = (e->boundingBox.minZ + e->boundingBox.maxZ) * 0.5f;

    e->ySlideOffset *= 0.4f;

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
    // CORRECTION: missing the real source's own -0.5f term (bytecode
    // confirmed: y + heightOffset/2 - 0.5f), shifting the sampled lighting
    // column up by half a block for every consumer: mobs, particles,
    // arrows, network players
    return Level_getBrightness(e->level, (int)e->x, (int)(e->y + e->heightOffset / 2.0f - 0.5f), (int)e->z);
}

bool Entity_shouldRender(const Entity* e, float camX, float camY, float camZ) {
    float dx = e->x - camX, dy = e->y - camY, dz = e->z - camZ;
    float size = AABB_getSize(&e->boundingBox) * 64.0f;
    return (dx * dx + dy * dy + dz * dz) < size * size;
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
