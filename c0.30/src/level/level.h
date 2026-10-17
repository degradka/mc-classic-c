// level/level.h: world storage, light columns, ticking and ray queries

#ifndef LEVEL_H
#define LEVEL_H

#include "common.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "../phys/aabb.h"
#include <math.h>
#include "../jrandom.h"

typedef unsigned char byte;

struct LevelRenderer; typedef struct LevelRenderer LevelRenderer;
struct Entity; typedef struct Entity Entity;

// a scheduled tile tick
// delay is how many more 5 tick rounds it waits, lava uses it
typedef struct {
    int x, y, z;
    int tileId;
    int delay;
} TickEntry;

typedef struct Level {
    int   width, height, depth;
    byte* blocks;
    int*  lightDepths;
    LevelRenderer* renderer;
    int unprocessed;

    // only saved and loaded with level.dat
    char name[64];
    char creator[64];
    long long createTime;

    int   xSpawn, ySpawn, zSpawn;
    float rotSpawn;

    // picks the tile tick positions
    unsigned int tickRandom;
    // Level.random, used by tile ticks and mob spawning
    JRandom random;
    int tickCount;

    TickEntry* tickList;
    int tickListSize;
    int tickListCapacity;

    // set for a level from a server, only its SetBlock packets change tiles
    bool networkMode;
    // set by the game mode, mobs take no damage while it's on
    bool creativeMode;
    // set by every game mode but creative, flowers stop ticking while it's on
    bool growTrees;

    // the local player, mobs chase it
    Entity* player;

    int skyColor, fogColor, cloudColor;
    int waterLevel;
} Level;

typedef struct {
    int   size;
    int   capacity;
    AABB* aabbs;
} ArrayList_AABB;

ArrayList_AABB Level_getCubes(const Level* level, const AABB* boundingBox);

// true when level.dat was loaded, false when it generated a new map
bool  Level_init(Level* level, int width, int height, int depth);
// the empty 8x8x8 level used while connecting to a server
void  Level_initPlaceholder(Level* level);
void  Level_destroy(Level* level);
// frees the existing blocks and lightDepths, allocates at the new size, and
// regenerates. Caller must rebuild the LevelRenderer afterward since its
// chunk grid is sized for the old dimensions.
void  Level_resize(Level* level, int width, int height, int depth);
void  Level_setDataFromNetwork(Level* level, int width, int height, int depth, const byte* blocks);

bool  Level_isTile(const Level* level, int x, int y, int z);
bool  Level_isSolidTile(const Level* level, int x, int y, int z);
bool  Level_isLightBlocker(const Level* level, int x, int y, int z);

void  calcLightDepths(Level* level, int minX, int minZ, int maxX, int maxZ);

void  Level_generateMap(Level* level);

void  Level_setBlocks(Level* level, int width, int height, int depth, byte* blocks);

// does nothing while networkMode is set
bool  level_setTile(Level* level, int x, int y, int z, int type);
// always sets the tile, the SetBlock packet uses it
bool  Level_netSetTile(Level* level, int x, int y, int z, int type);
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
void  Level_findSpawn(Level* level);
void  Level_setSpawnPos(Level* level, int x, int y, int z, float rot);

// no neighbor update at the source, used by sand and gravel
void  Level_swap(Level* level, int x1, int y1, int z1, int x2, int y2, int z2);

// the queue is drained every 5 ticks
void  Level_addToTickNextTick(Level* level, int x, int y, int z, int tileId);

// calls the tile's neighborChanged with its own id, the sponge uses it
void  Level_updateNeighborsAt(Level* level, int x, int y, int z, int type);
bool  Level_netSetTileNoNeighborChange(Level* level, int x, int y, int z, int type);
bool  Level_setTileNoNeighborChange(Level* level, int x, int y, int z, int type);

static inline void   Level_setPlayer(Level* level, Entity* player) { level->player = player; }
static inline Entity* Level_getPlayer(const Level* level) { return level->player; }

// true when a solid tile blocks the line between the points
bool Level_clip(const Level* level, float x1, float y1, float z1, float x2, float y2, float z2);
bool Level_clipHit(const Level* level, float* sx, float* sy, float* sz, float ex, float ey, float ez,
                   int* outX, int* outY, int* outZ, int* outFace, float* hitX, float* hitY, float* hitZ);

// places nothing unless the whole tree fits and there is grass below
bool Level_maybeGrowTree(Level* level, int x, int y, int z);

// source gets the credit for anything hurt
void Level_explode(Level* level, Entity* source, float x, float y, float z, float radius);

#endif  // LEVEL_H