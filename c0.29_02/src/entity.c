// entity.c: base entity, physics and movement

#include "entity.h"
#include "math_helper.h"
#include "level/tile/tile.h"
#include "audio/sound.h"
#include "mob.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define DEG2RAD(d) ((d) * M_PI / 180.0)

void Entity_init(Entity* e, Level* level) {
    e->level = level;
    e->x = e->y = e->z = 0.0f;
    e->xRotation = e->yRotation = 0.f;
    e->prevXRotation = e->prevYRotation = 0.f;
    e->motionX = e->motionY = e->motionZ = 0.0f;
    e->onGround = false;
    e->horizontalCollision = false;
    e->collision = false;
    e->heightOffset = 0.0f;
    e->boundingBoxWidth  = 0.6f;
    e->boundingBoxHeight = 1.8f;
    e->removed = false;
    e->walkDist = 0.0f;
    e->walkDistO = 0.0f;
    e->footSize = 0.0f;
    e->ySlideOffset = 0.0f;
    e->nextStep = 1;
    e->makeStepSound = true;

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
    e->afterAiStep = NULL;
    e->hurtOverride = NULL;
    e->noPhysics = false;
    e->slotted = false;
    e->inGrid = true;
    e->xOld = e->yOld = e->zOld = 0.0f;
    e->pushthrough = 0.0f;
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

// Entity.resetPos, climbs from the spawn until free. a spawn at y <= 0 leaves the position alone
void Entity_resetPosition(Entity* e) {
    float x = e->level->xSpawn + 0.5f;
    float z = e->level->zSpawn + 0.5f;

    for (float y = (float)e->level->ySpawn; y > 0.0f; y += 1.0f) {
        Entity_setPosition(e, x, y, z);
        ArrayList_AABB hits = Level_getCubes(e->level, &e->boundingBox);
        int blocked = hits.size != 0;
        if (hits.aabbs) free(hits.aabbs);
        if (!blocked) break;
    }

    e->motionX = e->motionY = e->motionZ = 0.0f;
    e->yRotation = e->level->rotSpawn;
    e->xRotation = 0.0f;
}

void Entity_turn(Entity* e, float dx, float dy) {
    float xo = e->xRotation, yo = e->yRotation;
    e->yRotation = (float)((double)e->yRotation + (double)dx * 0.15);
    e->xRotation = (float)((double)e->xRotation - (double)dy * 0.15);
    if (e->xRotation < -90.0f) e->xRotation = -90.0f;
    if (e->xRotation >  90.0f) e->xRotation =  90.0f;
    e->prevXRotation += e->xRotation - xo;
    e->prevYRotation += e->yRotation - yo;
}

void Entity_onTick(Entity* e) {
    e->prevX = e->x;
    e->prevY = e->y;
    e->prevZ = e->z;
    e->prevXRotation = e->xRotation;
    e->prevYRotation = e->yRotation;
    e->walkDistO = e->walkDist;
}

// clips against the cubes in y then x then z and moves the box
static void collide(Entity* e, float* dx, float* dy, float* dz) {
    AABB expanded = AABB_expand(&e->boundingBox, *dx, *dy, *dz);
    ArrayList_AABB hits = Level_getCubes(e->level, &expanded);
    for (int i = 0; i < hits.size; ++i) *dy = AABB_clipYCollide(&hits.aabbs[i], &e->boundingBox, *dy);
    AABB_move(&e->boundingBox, 0.0f, *dy, 0.0f);
    for (int i = 0; i < hits.size; ++i) *dx = AABB_clipXCollide(&hits.aabbs[i], &e->boundingBox, *dx);
    AABB_move(&e->boundingBox, *dx, 0.0f, 0.0f);
    for (int i = 0; i < hits.size; ++i) *dz = AABB_clipZCollide(&hits.aabbs[i], &e->boundingBox, *dz);
    AABB_move(&e->boundingBox, 0.0f, 0.0f, *dz);
    if (hits.aabbs) free(hits.aabbs);
}

void Entity_move(Entity* e, float dx, float dy, float dz) {
    if (e->noPhysics) {
        AABB_move(&e->boundingBox, dx, dy, dz);
        e->x = (e->boundingBox.minX + e->boundingBox.maxX) / 2.0f;
        e->y = e->boundingBox.minY + e->heightOffset - e->ySlideOffset;
        e->z = (e->boundingBox.minZ + e->boundingBox.maxZ) / 2.0f;
        return;
    }
    const float ox = dx, oy = dy, oz = dz;
    const float startX = e->x, startZ = e->z;
    const AABB start = e->boundingBox;

    // decided after the y clip, before x and z
    AABB expanded = AABB_expand(&e->boundingBox, dx, dy, dz);
    ArrayList_AABB hits = Level_getCubes(e->level, &expanded);
    for (int i = 0; i < hits.size; ++i) dy = AABB_clipYCollide(&hits.aabbs[i], &e->boundingBox, dy);
    AABB_move(&e->boundingBox, 0.0f, dy, 0.0f);
    bool grounded = e->onGround || (oy != dy && oy < 0.0f);
    for (int i = 0; i < hits.size; ++i) dx = AABB_clipXCollide(&hits.aabbs[i], &e->boundingBox, dx);
    AABB_move(&e->boundingBox, dx, 0.0f, 0.0f);
    for (int i = 0; i < hits.size; ++i) dz = AABB_clipZCollide(&hits.aabbs[i], &e->boundingBox, dz);
    AABB_move(&e->boundingBox, 0.0f, 0.0f, dz);

    // blocked sideways on the ground: try again from footSize higher, keep whichever goes further
    if (e->footSize > 0.0f && grounded && e->ySlideOffset < 0.05f && (ox != dx || oz != dz)) {
        float keepX = dx, keepY = dy, keepZ = dz;
        AABB moved = e->boundingBox;
        e->boundingBox = start;
        dx = ox;
        dy = e->footSize;
        dz = oz;
        collide(e, &dx, &dy, &dz);
        if (keepX * keepX + keepZ * keepZ >= dx * dx + dz * dz) {
            dx = keepX;
            dy = keepY;
            dz = keepZ;
            e->boundingBox = moved;
        } else {
            e->ySlideOffset = (float)((double)e->ySlideOffset + 0.5);
        }
    }

    e->horizontalCollision = !(ox == dx && oz == dz);
    e->onGround = (oy != dy) && (oy < 0.0f);
    e->collision = e->horizontalCollision || oy != dy;

    if (e->onGround) {
        if (e->fallDistance > 0.0f) {
            Mob_causeFallDamage(e, e->fallDistance);
            e->fallDistance = 0.0f;
        }
    } else if (dy < 0.0f) {
        e->fallDistance -= dy;
    }

    if (ox != dx) e->motionX = 0.0f;
    if (oy != dy) e->motionY = 0.0f;
    if (oz != dz) e->motionZ = 0.0f;

    e->x = (e->boundingBox.minX + e->boundingBox.maxX) * 0.5f;
    e->y =  e->boundingBox.minY + e->heightOffset - e->ySlideOffset;
    e->z = (e->boundingBox.minZ + e->boundingBox.maxZ) * 0.5f;

    // footsteps, a step is used up even on a tile with no sound
    float ddx = e->x - startX;
    float ddz = e->z - startZ;
    e->walkDist = (float)((double)e->walkDist + (double)MathHelper_sqrt(ddx * ddx + ddz * ddz) * 0.6);
    if (e->makeStepSound) {
        int tileId = Level_getTile(e->level, (int)e->x, (int)(e->y - 0.2f - e->heightOffset), (int)e->z);
        if (e->walkDist > (float)e->nextStep && tileId > 0) {
            e->nextStep++;
            const Tile* t = gTiles[tileId];
            if (t && t->soundType != SOUND_NONE) {
                char name[32];
                snprintf(name, sizeof name, "step.%s", SOUND_TYPES[t->soundType].name);
                Sound_playEntity(name, e, SoundType_getVolume(t->soundType) * 0.75f, SoundType_getPitch(t->soundType));
            }
        }
    }

    e->ySlideOffset *= 0.4f;
    if (hits.aabbs) free(hits.aabbs);
}

void Entity_moveRelative(Entity* e, float x, float z, float speed) {
    float d = MathHelper_sqrt(x * x + z * z);
    if (d < 0.01f) return;
    if (d < 1.0f) d = 1.0f;

    float k = speed / d;
    x *= k; z *= k;

    const float s = MathHelper_sin(e->yRotation * 3.1415927f / 180.0f);
    const float c = MathHelper_cos(e->yRotation * 3.1415927f / 180.0f);

    e->motionX += x * c - z * s;
    e->motionZ += z * c + x * s;
}

bool Entity_isLit(const Entity* e) {
    return Level_isLit(e->level, (int)e->x, (int)e->y, (int)e->z);
}

float Entity_getBrightness(const Entity* e) {
    return Level_getBrightness(e->level, (int)e->x, (int)(e->y + e->heightOffset / 2.0f - 0.5f), (int)e->z);
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
    AABB box = AABB_grow(&e->boundingBox, 0.0f, -0.4f, 0.0f);
    return Level_containsLiquid(e->level, &box, LIQUID_LAVA);
}

void Entity_remove(Entity* e) {
    e->removed = true;
}

void Entity_pushEntity(Entity* self, Entity* other) {
    float dx = other->x - self->x;
    float dz = other->z - self->z;
    float d = dx * dx + dz * dz;
    if (d >= 0.01f) {
        d = MathHelper_sqrt(d);
        dx /= d;
        dz /= d;
        dx /= d;
        dz /= d;
        dx *= 0.05f;
        dz *= 0.05f;
        dx *= 1.0f - self->pushthrough;
        dz *= 1.0f - self->pushthrough;
        self->motionX -= dx;
        self->motionZ -= dz;
        other->motionX += dx;
        other->motionZ += dz;
    }
}

void Entity_setSize(Entity* e, float width, float height) {
    e->boundingBoxWidth  = width;
    e->boundingBoxHeight = height;
}

static int gridSlot(float v, int cells) {
    int s = (int)(v / 16.0f);
    if (s < 0) s = 0;
    if (s >= cells) s = cells - 1;
    return s;
}

static int gridCells(int size) {
    int n = size / 16;
    return n == 0 ? 1 : n;
}

void Entity_gridCell(const Entity* e, int* cx, int* cy, int* cz) {
    float x = e->slotted ? e->xOld : e->x, y = e->slotted ? e->yOld : e->y, z = e->slotted ? e->zOld : e->z;
    *cx = gridSlot(x, gridCells(e->level->width));
    *cy = gridSlot(y, gridCells(e->level->depth));
    *cz = gridSlot(z, gridCells(e->level->height));
}

void Entity_gridInsert(Entity* e) {
    if (e->slotted) return;
    e->slotted = true;
    e->xOld = e->x;
    e->yOld = e->y;
    e->zOld = e->z;
}

void Entity_gridTick(Entity* e) {
    if ((int)(e->xOld / 16.0f) == (int)(e->x / 16.0f) && (int)(e->yOld / 16.0f) == (int)(e->y / 16.0f) &&
        (int)(e->zOld / 16.0f) == (int)(e->z / 16.0f)) return;
    // BlockMap.moved, nothing changes when both land in the same clamped slot
    int w = gridCells(e->level->width), d = gridCells(e->level->depth), h = gridCells(e->level->height);
    if (gridSlot(e->xOld, w) == gridSlot(e->x, w) && gridSlot(e->yOld, d) == gridSlot(e->y, d) &&
        gridSlot(e->zOld, h) == gridSlot(e->z, h)) return;
    e->inGrid = true;
    e->xOld = e->x;
    e->yOld = e->y;
    e->zOld = e->z;
}

// Entity.shouldRender, far small things aren't drawn
bool Entity_shouldRender(const Entity* e, float cx, float cy, float cz) {
    float dx = e->x - cx, dy = e->y - cy, dz = e->z - cz;
    float d = dx * dx + dy * dy + dz * dz;
    const AABB* b = &e->boundingBox;
    float size = ((b->maxX - b->minX) + (b->maxY - b->minY) + (b->maxZ - b->minZ)) / 3.0f;
    size *= 64.0f;
    return d < size * size;
}
