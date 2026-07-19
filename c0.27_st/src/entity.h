// entity.h: parent entity with physics and movement

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

    // c0.27_st: new auto-step mechanic, active when footSize>0 and horizontal
    // movement was blocked while grounded, Entity_move retries the move
    // allowing up to footSize of vertical rise, letting mobs climb a half
    // block ledge without jumping. ySlideOffset smooths the visual step
    // over a few ticks (subtracted from render height, decaying toward 0)
    // rather than snapping instantly. Default 0 (matches every non-Mob
    // entity); Mob_init sets 0.5 for Player and every hostile/passive mob
    float  footSize;
    float  ySlideOffset;

    // c0.0.23a_01: horizontal distance walked, accumulated in Entity_move and
    // used to space out footstep sounds. makeStepSound defaults true, Particle
    // is the only entity in the real source that disables it
    float walkDist;
    bool  makeStepSound;

    // c0.24_st_03: Mob state (health/damage/knockback/timers), matching the
    // real source's Player now extending a new Mob class. Added directly to
    // Entity rather than as a separate wrapping struct, same reasoning as
    // prevXRotation/prevYRotation above: it avoids turning every existing
    // player.e.x / zombie.base.x access into a deeper player.mob.e.x chain.
    // isMob is false (fields inert/unused) for Particle and NetworkPlayer;
    // Mob_init sets it true for Player and every hostile/passive mob
    bool  isMob;
    int   health;
    int   lastHealth;
    int   invulnerableTime;
    int   hurtTime, hurtDuration;
    // hurt flash camera and model wobble direction, in degrees relative to
    // yRotation. Set by Mob_hurt to the angle toward the attacker, or a
    // random 0/180 flip for environmental damage, bytecode verified against
    // Mob.hurt() and Mob.knockback()
    float hurtDir;
    int   deathTime;
    int   attackTime;
    int   airSupply;
    float yBodyRotation, prevYBodyRotation;
    float fallDistance;
    bool  dead;
    // walk cycle drivers for a Mob's own rendered model, head bob and limb
    // swing. Unused by Player and NetworkPlayer, which each keep their own
    // separate copies of this same idea locally instead, see player_model
    // rendering and network_player.c respectively
    int   tickCount;
    float run, oRun;
    float animStep, animStepO;
    // fired once, the tick health first reaches <=0. NULL for entities with
    // no special death behavior beyond Mob's own generic handling
    void (*onDeath)(struct Entity* self, struct Entity* attacker);
    // wander, chase, and attack strategy, matches Mob.ai. NULL for Player,
    // whose own movement is key driven, see player.c, and for anything that
    // isn't a Mob at all; every hostile or passive mob owns one. Its own
    // onDeathTimeout callback in mob_ai.h is what Mob_onTick fires 20 ticks
    // after death, matching the real source's `if (ai != null) ai.a();`
    // right before removal, for example Creeper's explosion
    Ai* ai;
    // redirects Mob_hurt's score crediting to a different entity than the
    // actual attacker passed in. NULL for everything except Arrow, which
    // must itself be the attacker so hurtDir and knockback resolve relative
    // to where the arrow struck, not wherever the archer is standing, while
    // still crediting the shooting player's kill score. Matches the real
    // source's Arrow.awardKillScore() forwarding to its own owner, a virtual
    // dispatch this port handles this way instead
    Entity* killCredit;
    // c0.25_05_st: a lightweight stand in for Java's getClass() equality
    // check, used by BasicAttackAI.hurt() to decide whether an attacker
    // counts as a different kind of thing worth aggroing onto (mobs fighting
    // each other, but not a zombie attacking another zombie). AI_CLASS_PLAYER
    // for Player, the entity's own CreatureKind value for every hostile or
    // passive mob, unused and left at its default for anything else (Item,
    // Arrow, Particle, NetworkPlayer)
    int aiClassTag;
} Entity;

// c0.25_05_st: Entity.aiClassTag values, matches Java's getClass() equality
// check inside BasicAttackAI.hurt() well enough for this port's own small,
// fixed set of concrete attacker types. Creature's own kind values are
// appended after PLAYER at Creature_init time (0 plus CreatureKind), so this
// only needs the one reserved sentinel here
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
// c0.27_st: matches Entity.shouldRender(Vec3)/shouldRenderAtSqrDistance(float)
// exactly: squared distance from the camera to this entity's own position
// (not its bounding box center) against (bbSize*64)^2, a per-entity-size
// render distance cull independent of the frustum test. Was entirely
// missing, so every entity rendered at any distance as long as it was in the
// view frustum, regardless of how small or far away it actually was
bool Entity_shouldRender(const Entity* e, float camX, float camY, float camZ);
void Entity_remove(Entity* e);
void Entity_setSize(Entity* e, float width, float height);
bool Entity_isFree(const Entity* e, float dx, float dy, float dz);
bool Entity_isInWater(const Entity* e);
// distinct from isInWater: checks only the single tile at eye level,
// (x, y+0.12, z), for water specifically, matching Entity.isUnderWater()
// exactly. isInWater is a much broader test for whether any part of the
// grown bounding box overlaps some water, used for swim movement and
// fall distance reset. This one gates drowning, so standing chest or waist
// deep without your eyes actually in a water block doesn't consume air
bool Entity_isUnderWater(const Entity* e);
bool Entity_isInLava(const Entity* e);

#endif
