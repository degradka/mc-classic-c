// level/levelgen/level_gen.h: flat terrain, carved caves and flood filled lakes
// the heightmap step does nothing yet, so the synth pipeline isn't used

#ifndef LEVEL_GEN_H
#define LEVEL_GEN_H

#include "../level.h"

bool LevelGen_generateLevel(Level* level, const char* userName, int width, int height, int depth);

#endif
