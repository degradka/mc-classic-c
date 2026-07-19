// level/levelgen/level_gen.h: perlin terrain, carved caves and flood filled lakes

#ifndef LEVEL_GEN_H
#define LEVEL_GEN_H

#include "../level.h"

bool LevelGen_generateLevel(Level* level, const char* userName, int width, int height, int depth);

// the 16 block distance is always from the spawn point
// reportProgress is only for the loading bar
int LevelGen_maybeSpawnMobs(Level* level, int attempts, bool reportProgress);

#endif
