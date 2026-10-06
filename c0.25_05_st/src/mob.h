// mob.h: health, damage, knockback and death

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

// health 20, air 300
void Mob_init(Entity* e);

// hurt timers, death, drowning, lava and the body yaw, does nothing unless isMob
void Mob_onTick(Entity* e);

// damage is ceil(fallDistance - 3)
void Mob_causeFallDamage(Entity* e, float fallDistance);

// attacker is NULL for falls, drowning and lava
void Mob_hurt(Entity* e, Entity* attacker, int damage);
void Mob_heal(Entity* e, int amount);

// halves the velocity, then pushes 0.4 away from the attacker and 0.4 up
void Mob_knockback(Entity* e, Entity* attacker);

// Mob.travel, the ai moves with it, the player has its own copy
void Mob_travel(Entity* e, float strafe, float forward);

#endif
