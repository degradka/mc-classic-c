// entity.c: base entity, physics and movement

#include "entity.h"
#include "jrandom.h"
#include <math.h>
#include <stdlib.h>

void Entity_init(Entity* e, Level* level) {
    e->level = level;
    e->x = e->y = e->z = 0.0f;
    e->prevX = e->prevY = e->prevZ = 0.0f;
    e->motionX = e->motionY = e->motionZ = 0.0f;
    e->xRotation = e->yRotation = 0.0f;
    e->onGround = false;
    e->heightOffset = 0.0f;
    e->boundingBoxWidth  = 0.6f;
    e->boundingBoxHeight = 1.8f;
    e->removed = false;

    Entity_resetPosition(e);
}

void Entity_setPosition(Entity* e, float x, float y, float z) {
    e->x = x; e->y = y; e->z = z;
    float w = e->boundingBoxWidth / 2.0f;
    float h = e->boundingBoxHeight / 2.0f;
    e->boundingBox = AABB_create(x - w, y - h, z - w, x + w, y + h, z + w);
}

void Entity_resetPosition(Entity* e) {
    float x = (float)JRandom_mathRandom() * (float)e->level->width;
    float y = (float)(e->level->depth + 10);
    float z = (float)JRandom_mathRandom() * (float)e->level->height;
    Entity_setPosition(e, x, y, z);
}

void Entity_turn(Entity* e, float dx, float dy) {
    e->yRotation = (float)(e->yRotation + dx * 0.15);
    e->xRotation = (float)(e->xRotation - dy * 0.15);
    if (e->xRotation < -90.0f) e->xRotation = -90.0f;
    if (e->xRotation > 90.0f) e->xRotation = 90.0f;
}

void Entity_onTick(Entity* e) {
    e->prevX = e->x;
    e->prevY = e->y;
    e->prevZ = e->z;
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
    float dist = x * x + z * z;
    if (dist < 0.01f) return;

    dist = speed / (float)sqrt(dist);
    x *= dist;
    z *= dist;

    float s = (float)sin(e->yRotation * M_PI / 180.0);
    float c = (float)cos(e->yRotation * M_PI / 180.0);

    e->motionX += x * c - z * s;
    e->motionZ += z * c + x * s;
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
