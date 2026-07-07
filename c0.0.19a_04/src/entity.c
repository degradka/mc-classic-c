// entity.c: base entity, physics and movement

#include "entity.h"
#include "jrandom.h"
#include "level/tile/tile.h"
#include <math.h>
#include <stdlib.h>

void Entity_init(Entity* e, Level* level) {
    e->level = level;
    e->x = e->y = e->z = 0.0f;
    e->prevX = e->prevY = e->prevZ = 0.0f;
    e->motionX = e->motionY = e->motionZ = 0.0f;
    e->xRotation = e->yRotation = 0.0f;
    e->xRotO = e->yRotO = 0.0f;
    e->onGround = false;
    e->horizontalCollision = false;
    e->heightOffset = 0.0f;
    e->boundingBoxWidth  = 0.6f;
    e->boundingBoxHeight = 1.8f;
    e->removed = false;

    Entity_setPosition(e, 0.0f, 0.0f, 0.0f);
}

void Entity_setPosition(Entity* e, float x, float y, float z) {
    e->x = x; e->y = y; e->z = z;
    float w = e->boundingBoxWidth / 2.0f;
    float h = e->boundingBoxHeight / 2.0f;
    e->boundingBox = AABB_create(x - w, y - h, z - w, x + w, y + h, z + w);
}

// climbs from the spawn until the box is free, a failed spawn search (y -100) never moves it
void Entity_resetPosition(Entity* e) {
    if (!e->level) return;
    float x = (float)e->level->xSpawn + 0.5f;
    float y = (float)e->level->ySpawn;
    float z = (float)e->level->zSpawn + 0.5f;
    while (y > 0.0f) {
        Entity_setPosition(e, x, y, z);
        ArrayList_AABB cubes = Level_getCubes(e->level, &e->boundingBox);
        int n = cubes.size;
        free(cubes.aabbs);
        if (n == 0) break;
        y += 1.0f;
    }
    e->motionX = e->motionY = e->motionZ = 0.0f;
    e->yRotation = e->level->rotSpawn;
    e->xRotation = 0.0f;
}

void Entity_turn(Entity* e, float dx, float dy) {
    float xr = e->xRotation;
    float yr = e->yRotation;
    e->yRotation = (float)(e->yRotation + dx * 0.15);
    e->xRotation = (float)(e->xRotation - dy * 0.15);
    if (e->xRotation < -90.0f) e->xRotation = -90.0f;
    if (e->xRotation > 90.0f) e->xRotation = 90.0f;
    e->xRotO += e->xRotation - xr;
    e->yRotO += e->yRotation - yr;
}

void Entity_moveTo(Entity* e, float x, float y, float z, float yRot, float xRot) {
    e->prevX = e->x = x;
    e->prevY = e->y = y;
    e->prevZ = e->z = z;
    e->yRotation = yRot;
    e->xRotation = xRot;
    Entity_setPosition(e, x, y, z);
}

void Entity_setPosUpdate(Entity* e, const PositionUpdate* u) {
    if (u->hasPos) Entity_setPosition(e, u->x, u->y, u->z);
    else Entity_setPosition(e, e->x, e->y, e->z);
    if (u->hasRot) {
        e->yRotation = u->yRot;
        e->xRotation = u->xRot;
    }
}

void Entity_onTick(Entity* e) {
    e->prevX = e->x;
    e->prevY = e->y;
    e->prevZ = e->z;
    e->xRotO = e->xRotation;
    e->yRotO = e->yRotation;
}

void Entity_move(Entity* e, float dx, float dy, float dz) {
    float ox = dx, oy = dy, oz = dz;

    AABB expanded = AABB_expand(&e->boundingBox, dx, dy, dz);
    ArrayList_AABB hits = Level_getCubes(e->level, &expanded);

    for (int i = 0; i < hits.size; ++i) dy = AABB_clipYCollide(&hits.aabbs[i], &e->boundingBox, dy);
    AABB_move(&e->boundingBox, 0.0f, dy, 0.0f);

    for (int i = 0; i < hits.size; ++i) dx = AABB_clipXCollide(&hits.aabbs[i], &e->boundingBox, dx);
    AABB_move(&e->boundingBox, dx, 0.0f, 0.0f);

    for (int i = 0; i < hits.size; ++i) dz = AABB_clipZCollide(&hits.aabbs[i], &e->boundingBox, dz);
    AABB_move(&e->boundingBox, 0.0f, 0.0f, dz);

    e->horizontalCollision = !(ox == dx && oz == dz);
    e->onGround = oy != dy && oy < 0.0f;

    if (ox != dx) e->motionX = 0.0f;
    if (oy != dy) e->motionY = 0.0f;
    if (oz != dz) e->motionZ = 0.0f;

    e->x = (e->boundingBox.minX + e->boundingBox.maxX) / 2.0f;
    e->y = e->boundingBox.minY + e->heightOffset;
    e->z = (e->boundingBox.minZ + e->boundingBox.maxZ) / 2.0f;

    free(hits.aabbs);
}

void Entity_moveRelative(Entity* e, float x, float z, float speed) {
    float dist = (float)sqrt(x * x + z * z);
    if (dist < 0.01f) return;
    if (dist < 1.0f) dist = 1.0f;

    dist = speed / dist;
    x *= dist;
    z *= dist;

    float s = (float)sin(e->yRotation * M_PI / 180.0);
    float c = (float)cos(e->yRotation * M_PI / 180.0);

    e->motionX += x * c - z * s;
    e->motionZ += z * c + x * s;
}

bool Entity_isFree(const Entity* e, float dx, float dy, float dz) {
    AABB box = AABB_cloneMove(&e->boundingBox, dx, dy, dz);
    ArrayList_AABB hits = Level_getCubes(e->level, &box);
    bool blocked = hits.size > 0;
    free(hits.aabbs);
    if (blocked) return false;
    if (Level_containsAnyLiquid(e->level, &box)) return false;
    return true;
}

bool Entity_isInWater(const Entity* e) {
    AABB box = AABB_grow(&e->boundingBox, 0.0f, -0.4f, 0.0f);
    return Level_containsLiquid(e->level, &box, LIQUID_WATER);
}

bool Entity_isInLava(const Entity* e) {
    AABB box = AABB_grow(&e->boundingBox, 0.0f, -0.4f, 0.0f);
    return Level_containsLiquid(e->level, &box, LIQUID_LAVA);
}

float Entity_getBrightness(const Entity* e) {
    return Level_getBrightness(e->level, (int)e->x, (int)(e->y + e->heightOffset / 2.0f), (int)e->z);
}

bool Entity_isLit(const Entity* e) {
    return Level_isLit(e->level, (int)e->x, (int)e->y, (int)e->z);
}

void Entity_remove(Entity* e) {
    e->removed = true;
}

void Entity_setSize(Entity* e, float width, float height) {
    e->boundingBoxWidth  = width;
    e->boundingBoxHeight = height;
}
