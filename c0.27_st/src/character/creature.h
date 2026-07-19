// character/creature.h: creature AI and state for zombies, skeletons, creepers, pigs, spiders and sheep

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
    CREATURE_SPIDER,
} CreatureKind;

typedef struct Creature {
    Entity e; // must be first so Creature* can be used as Entity*
    Ai ai;
    CreatureKind kind;

    // helmet and armor, only zombies and skeletons use them
    bool hasHelmet;
    bool hasArmor;
} Creature;

void Creature_init(Creature* c, CreatureKind kind, Level* level, float x, float y, float z);
void Creature_onTick(Creature* c);
void Creature_render(const Creature* c, float partialTicks);

#endif
