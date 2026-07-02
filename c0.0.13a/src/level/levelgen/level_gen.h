// level/levelgen/level_gen.h — c0.0.13a world generator: flat terrain + carved caves + flood-filled lakes
//
// Note: in the real c0.0.13a source, buildHeightmap() is a no-op and buildBlocks()
// never reads it — the Synth/PerlinNoise pipeline exists in the source but isn't
// wired into terrain shape yet (a transitional "cave update" build). Ported
// faithfully: this generates flat terrain, not rolling hills.

#ifndef LEVEL_GEN_H
#define LEVEL_GEN_H

#include "../level.h"

// Fills level->blocks (already allocated by Level_init) in place.
void LevelGen_generateMap(Level* level);

#endif
