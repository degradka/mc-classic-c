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

// matches Level.maybeSpawnMobs(int,Entity,ProgressListener): the shared mob
// population routine, reused both by world gen's own one time population
// pass and by Level_onTick's own small periodic top up while playing. The
// new 16 block exclusion zone (256 squared) is always measured from the
// level's own fixed spawn point; real source's own reference-entity
// parameter is dead code (its computed distance gets unconditionally
// overwritten by the spawn point one before ever being used, verified by
// direct read of Level.java), so this port drops the parameter entirely
// rather than keep an argument real behavior never reads.
// reportProgress drives the loading screen progress bar and must only be
// true for the world gen call. Returns how many mobs actually got placed
int LevelGen_maybeSpawnMobs(Level* level, int attempts, bool reportProgress);

#endif
