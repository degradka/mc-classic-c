// entity.h: base entity, physics and movement

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
    // the rotation at the last tick, turning moves these along with it
    float xRotO, yRotO;

    AABB  boundingBox;
    float boundingBoxWidth;
    float boundingBoxHeight;

    bool  onGround;
    bool  horizontalCollision;
    float heightOffset;
    bool  removed;
    float walkDist;
    bool  makeStepSound;
    // sounds still following it, the level hands it to the sound code instead of freeing it
    int   soundRefs;
    bool  soundReleased;
} Entity;

void Entity_init(Entity* e, Level* level);
void Entity_setPosition(Entity* e, float x, float y, float z);
void Entity_resetPosition(Entity* e);
void Entity_turn(Entity* e, float dx, float dy);
void Entity_moveTo(Entity* e, float x, float y, float z, float yRot, float xRot);
// a position update from the network
typedef struct {
    float x, y, z, yRot, xRot;
    bool hasRot, hasPos;
} PositionUpdate;
void Entity_setPosUpdate(Entity* e, const PositionUpdate* u);
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
void Entity_playSound(Entity* e, const char* name, float volume, float pitch);

#endif
