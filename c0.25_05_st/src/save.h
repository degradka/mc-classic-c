// save.h: level.dat, version 1 is read, version 2 is read and written

#ifndef SAVE_H
#define SAVE_H

#include <stdbool.h>
#include "level/level.h"
#include "player.h"
#include "character/creature.h"
#include "item/item.h"
#include "item/arrow.h"
#include "item/sign.h"

typedef struct World {
    Level* level;
    Player* player;
    Creature* mobs; int* mobCount; int maxMobs;
    Item* items; int* itemCount; int maxItems;
    Arrow* arrows; int* arrowCount; int maxArrows;
    Sign* signs; int* signCount; int maxSigns;
} World;

// loads the level part, a version 2 file keeps its entities for Save_loadEntities
bool Save_loadLevel(Level* level, const char* path);
// adds the entities of the file Save_loadLevel just read, true if it had a player
bool Save_loadEntities(World* w);
bool Save_write(const World* w, const char* path);

#endif
