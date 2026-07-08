// entity.h: parent entity with physics and movement

#ifndef ENTITY_H
#define ENTITY_H

#include "level/level.h"
#include "phys/aabb.h"
#include <stdbool.h>

typedef struct Entity {
    Level* level;

    float x, y, z;
    float prevX, prevY, prevZ;
    float motionX, motionY, motionZ;
    float xRotation, yRotation;
    // matches Entity.xRotO/yRotO: only NetworkPlayer interpolates rotation
    // for rendering so far, everything else just leaves these unused
    float prevXRotation, prevYRotation;

    AABB   boundingBox;
    float boundingBoxWidth;
    float boundingBoxHeight;

    bool   onGround;
    bool   horizontalCollision;
    float  heightOffset;
    bool   removed;

    // c0.0.23a_01: horizontal distance walked, accumulated in Entity_move and
    // used to space out footstep sounds. makeStepSound defaults true, Particle
    // is the only entity in the real source that disables it
    float walkDist;
    bool  makeStepSound;
} Entity;

void Entity_init(Entity* e, Level* level);
void Entity_setPosition(Entity* e, float x, float y, float z);
void Entity_resetPosition(Entity* e);
void Entity_turn(Entity* e, float dx, float dy);
void Entity_onTick(Entity* e);
void Entity_move(Entity* e, float dx, float dy, float dz);
void Entity_moveRelative(Entity* e, float x, float z, float speed);
bool Entity_isLit(const Entity* e);
float Entity_getBrightness(const Entity* e);
void Entity_remove(Entity* e);
void Entity_setSize(Entity* e, float width, float height);
bool Entity_isFree(const Entity* e, float dx, float dy, float dz);
bool Entity_isInWater(const Entity* e);
bool Entity_isInLava(const Entity* e);

#endif
