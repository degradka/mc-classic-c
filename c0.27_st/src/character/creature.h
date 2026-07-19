// creature.h: a concrete spawnable AI driven mob, c0.24_st_03, Zombie,
// Skeleton, Creeper, or Pig. Ties an Entity plus Mob health and damage to
// an Ai for wander, chase, and attack, and a model and texture, replacing
// the old debug only character/zombie.c, which had no health or AI at all,
// see its own header

#ifndef CREATURE_H
#define CREATURE_H

#include "../entity.h"
#include "../mob_ai.h"
#include <stdbool.h>

typedef enum {
    CREATURE_ZOMBIE,
    CREATURE_SKELETON,
    CREATURE_CREEPER,
    CREATURE_PIG,
    CREATURE_SPIDER, // c0.27_st: new hostile mob, see mob_spider_model.h
} CreatureKind;

typedef struct Creature {
    Entity e; // first member: a Creature* can be passed where an Entity* is expected
    Ai ai;
    CreatureKind kind;

    // c0.27_st: matches HumanoidMob's own per-instance 20% independent random
    // rolls at construction, only meaningful for Zombie/Skeleton (the only
    // kinds that extend HumanoidMob in real source); left false for
    // Pig/Creeper/Spider, which never read them
    bool hasHelmet;
    bool hasArmor;
} Creature;

void Creature_init(Creature* c, CreatureKind kind, Level* level, float x, float y, float z);
void Creature_onTick(Creature* c);
void Creature_render(const Creature* c, float partialTicks);

#endif
