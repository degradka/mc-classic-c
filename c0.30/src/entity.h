// entity.h: base entity, physics and movement

#ifndef ENTITY_H
#define ENTITY_H

#include "level/level.h"
#include "phys/aabb.h"
#include <stdbool.h>

struct Ai; typedef struct Ai Ai;

typedef struct Entity {
    Level* level;

    float x, y, z;
    float prevX, prevY, prevZ;
    float motionX, motionY, motionZ;
    float xRotation, yRotation;
    float prevXRotation, prevYRotation;

    AABB   boundingBox;
    float boundingBoxWidth;
    float boundingBoxHeight;

    bool   onGround;
    bool   horizontalCollision;
    float  heightOffset;
    bool   removed;

    // walk distance spaces out the footsteps, particles make no step sound
    float walkDist, walkDistO;
    int nextStep;
    bool  makeStepSound;

    // Mob fields, only used when isMob is set
    bool  isMob;
    int   health;
    int   lastHealth;
    int   invulnerableTime;
    int   hurtTime, hurtDuration;
    // hurt wobble direction relative to yRotation, toward the attacker or a random 0 or 180
    float hurtDir;
    int   deathTime;
    int   attackTime;
    int   airSupply;
    float yBodyRotation, prevYBodyRotation;
    float fallDistance;
    bool  dead;
    // walk cycle for the mob models
    int   tickCount;
    float run, oRun;
    bool collision;
    float footSize, ySlideOffset; // step up, mobs can walk up half a block
    bool noPhysics;    // move just shifts the box
    // BlockMap: the position it was last slotted at, and whether its grid cell holds it.
    // creative takes everything but the player out of the grid only, those keep
    // ticking unseen until they walk into another 16 block slot
    bool slotted, inGrid;
    float xOld, yOld, zOld;
    float pushthrough; // how much of a push goes through, network players
    float renderOffset; // Mob.renderOffset, network players are drawn this much higher
    float timeOffs, mobRot, rotA; // Mob fields only kept for the save
    float animStep, animStepO;
    // fired once, the tick health first reaches 0
    void (*onDeath)(struct Entity* self, struct Entity* attacker);
    // runs right after the ai tick in Mob.aiStep, the sheep's graze
    void (*afterAiStep)(struct Entity* self);
    // a hurt override that runs before Mob.hurt, true when it took the hit (sheep shearing)
    bool (*hurtOverride)(struct Entity* self, struct Entity* attacker);
    // Mob.ai, NULL for the player and anything that isn't a mob
    Ai* ai;
    // who gets the kill score, arrows credit their shooter
    Entity* killCredit;
    // the same kind doesn't fight
    // AI_CLASS_PLAYER for the player, the CreatureKind for creatures
    int aiClassTag;
} Entity;

#define AI_CLASS_NONE   (-2) // default, not a Player or a Creature
#define AI_CLASS_PLAYER (-1)

void Entity_init(Entity* e, Level* level);
void Entity_setPosition(Entity* e, float x, float y, float z);
void Entity_resetPosition(Entity* e);
void Entity_turn(Entity* e, float dx, float dy);
void Entity_onTick(Entity* e);
void Entity_move(Entity* e, float dx, float dy, float dz);
void Entity_moveRelative(Entity* e, float x, float z, float speed);
bool Entity_isLit(const Entity* e);
float Entity_getBrightness(const Entity* e);
// BlockMap.insert the first time, then BlockMap.tickAll's slot check after each tick
void Entity_gridInsert(Entity* e);
void Entity_gridTick(Entity* e);
// the grid cell the BlockMap keeps it in
void Entity_gridCell(const Entity* e, int* cx, int* cy, int* cz);
bool  Entity_shouldRender(const Entity* e, float cx, float cy, float cz);
void Entity_remove(Entity* e);
// Entity.push(Entity): both get shoved apart, 0.05 divided by the squared distance
void Entity_pushEntity(Entity* self, Entity* other);
void Entity_setSize(Entity* e, float width, float height);
bool Entity_isFree(const Entity* e, float dx, float dy, float dz);
bool Entity_isInWater(const Entity* e);
// only the tile at eye level, drowning uses it
bool Entity_isUnderWater(const Entity* e);
bool Entity_isInLava(const Entity* e);

#endif
