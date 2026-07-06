// level_gen.h: c0.0.13a_03 world generator, rolling hills, carved caves, flood filled lakes
//
// Unlike c0.0.13a, this version wires the Synth/PerlinNoise pipeline into
// real terrain shape: two distorted Perlin fields blended through a third
// noise field, then an erosion pass, drive a per column heightmap instead
// of a flat plateau.

#ifndef LEVEL_GEN_H
#define LEVEL_GEN_H

#include "../level.h"

// Fills level->blocks (already allocated by Level_init) in place.
void LevelGen_generateMap(Level* level);

#endif
