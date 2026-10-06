// level/levelgen/level_gen.h

#ifndef LEVEL_GEN_H
#define LEVEL_GEN_H

#include "../level.h"

void LevelGen_generateMap(Level* level);

// referenceEntity keeps new mobs 16 blocks away, NULL uses the spawn point
// reportProgress is only for the loading bar
int LevelGen_maybeSpawnMobs(Level* level, int attempts, const Entity* referenceEntity, bool reportProgress);

#endif
