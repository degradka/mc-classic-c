// mob.h: shared health/damage/knockback/invulnerability state and behavior,
// matching the real source's new Mob class (c0.24_st_03). Operates on a
// plain Entity*, not a wrapping struct: see the comment above Entity's own
// Mob fields in entity.h for why. Used by Player directly, and will be used
// by Zombie/Skeleton/Creeper/Pig once their own tasks port them onto it.

#ifndef MOB_H
#define MOB_H

#include "entity.h"
#include <stdbool.h>

#define MOB_MAX_HEALTH            20
#define MOB_INVULNERABLE_DURATION 30
#define MOB_HURT_DURATION         10
#define MOB_ATTACK_DURATION       5
#define MOB_TOTAL_AIR_SUPPLY      300
#define MOB_DEATH_REMOVE_TICKS    20 // ticks after health<=0 before the entity is actually removed

// marks e as a Mob, health 20, airSupply 300, isMob true. Call once, right
// after Entity_init, from Player_init and every hostile or passive mob's init
void Mob_init(Entity* e);

// per tick: hurt and invulnerability timer decay, death countdown and
// removal, drowning damage of 2 per tick once air runs out and lava damage
// of 10 per tick, and the decoupled body versus head yaw, yBodyRotation,
// used by mob models that keep looking at a target while still turning
// their body to walk. No op if !e->isMob. Call once per tick, before
// movement and AI for that entity
void Mob_onTick(Entity* e);

// call from Entity_move right when a landing is detected, oy<0 landing this
// call, passing the accumulated fall distance for this fall. No op if
// !e->isMob or the fall was 3 blocks or less, matches the real source's
// n = ceil(fallDistance-3.0), only damages if n>0
void Mob_causeFallDamage(Entity* e, float fallDistance);

// applies damage from attacker, NULL for environmental sources such as
// fall, drown, or lava, respecting the invulnerability window, triggers
// hurt animation state, knockback, and death, calling e->onDeath once, the
// tick health first reaches <=0. No op if !e->isMob
void Mob_hurt(Entity* e, Entity* attacker, int damage);

// halves current velocity, then displaces away from attacker by a
// normalized push of magnitude 0.4 plus a flat 0.4 vertical hop
void Mob_knockback(Entity* e, Entity* attacker);

// AI driven movement, matches Mob.travel(strafe,forward) exactly, confirmed
// identical formulas to Player_onTick's own water, lava, and ground
// movement branches, just packaged as a function the Ai objects can call
// instead of reading live key state. Not used by Player, which keeps its
// own separate key driven copy of this same logic
void Mob_travel(Entity* e, float strafe, float forward);

#endif
