// level.h: world storage, lighting columns, IO, and solid cube queries

#ifndef LEVEL_H
#define LEVEL_H

#include "common.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "../phys/aabb.h"
#include <math.h>

typedef unsigned char byte;

struct LevelRenderer; typedef struct LevelRenderer LevelRenderer;

// a pending liquid/gravity-tile reaction scheduled for a future tick, see
// Level_addToTickNextTick
typedef struct {
    int x, y, z;
    int tileId;
} TickEntry;

typedef struct Level {
    int   width, height, depth;
    byte* blocks;
    int*  lightDepths;
    LevelRenderer* renderer;  // backref used for dirty notifications
    int unprocessed;

    // save metadata, round tripped through level.dat but not used elsewhere
    char name[64];
    char creator[64];
    long long createTime;

    // c0.0.14a_08: a real spawn point instead of scattering entities randomly
    int   xSpawn, ySpawn, zSpawn;
    float rotSpawn;

    // c0.0.14a_08: the packed tick-random LCG state (Level.d in the real
    // source), separate from libc rand() used for everything else
    unsigned int tickRandom;
    int tickCount;

    TickEntry* tickList;
    int tickListSize;
    int tickListCapacity;
} Level;

typedef struct {
    int   size;
    int   capacity;
    AABB* aabbs;
} ArrayList_AABB;

ArrayList_AABB Level_getCubes(const Level* level, const AABB* boundingBox);

void  Level_init(Level* level, int width, int height, int depth);
void  Level_destroy(Level* level);
// frees the existing blocks and lightDepths, allocates at the new size, and
// regenerates. Caller must rebuild the LevelRenderer afterward since its
// chunk grid is sized for the old dimensions.
void  Level_resize(Level* level, int width, int height, int depth);

bool  Level_isTile(const Level* level, int x, int y, int z);
bool  Level_isSolidTile(const Level* level, int x, int y, int z);
bool  Level_isLightBlocker(const Level* level, int x, int y, int z);

void  calcLightDepths(Level* level, int minX, int minZ, int maxX, int maxZ);

void  Level_generateMap(Level* level);

bool  Level_load(Level* level);
void  Level_save(const Level* level);

bool  level_setTile(Level* level, int x, int y, int z, int type);
bool  Level_setTileNoUpdate(Level* level, int x, int y, int z, int type);
int   Level_getTile(const Level* level, int x, int y, int z);

AABB Level_getTilePickAABB(const Level* level, int x, int y, int z);

bool  Level_isLit(const Level* level, int x, int y, int z);
float Level_getBrightness(const Level* level, int x, int y, int z);

bool  Level_containsAnyLiquid(const Level* level, const AABB* box);
bool  Level_containsLiquid(const Level* level, const AABB* box, int liquidId);

void  Level_onTick(Level* level);

int   Level_getHighestTile(const Level* level, int x, int z);
float Level_getGroundLevel(const Level* level);
float Level_getWaterLevel(const Level* level);
// picks a spawn point on a column near the map center, above water level.
// Called automatically after Level_generateMap/Level_load.
void  Level_findSpawn(Level* level);
void  Level_setSpawnPos(Level* level, int x, int y, int z, float rot);

// swaps two tile positions with no neighbor notification of the source cell,
// used by the falling-block (Sand/Gravel) tile family
void  Level_swap(Level* level, int x1, int y1, int z1, int x2, int y2, int z2);

// schedules a tile reaction for a future tick (drained every 5 ticks), used
// by liquids and the falling-block tile family instead of resolving inline
void  Level_addToTickNextTick(Level* level, int x, int y, int z, int tileId);

#endif  // LEVEL_H