// level/level.h: world storage, light columns, ticking, entities and box queries

#ifndef LEVEL_H
#define LEVEL_H

#include "common.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "../phys/aabb.h"
#include "../jrandom.h"
#include <math.h>

typedef unsigned char byte;

struct LevelRenderer; typedef struct LevelRenderer LevelRenderer;
struct Zombie;
struct NetworkPlayer;

// a scheduled tile tick, only run if the tile there still has this id
typedef struct {
    int x, y, z, id;
    // rounds left before it runs
    int delay;
} TickEntry;

typedef struct Level {
    int   width, height, depth;
    byte* blocks;
    int*  lightDepths;
    LevelRenderer* renderer;
    JRandom random;
    int randValue;
    int unprocessed;
    int tickCount;

    TickEntry* tickList;
    int tickListSize, tickListCapacity;

    // zombies and network players never touch each other, so they're kept in two lists
    // minecraft.c owns the network players, the id map can still hold them after they leave a level
    struct Zombie* entities;
    int entityCount, entityCapacity;
    struct NetworkPlayer** players;
    int playerCount, playerCapacity;
    // the length of the array behind the entity list, level.dat keeps it
    int listCapacity;

    // a server's level, the local tile updates are off
    bool networkMode;

    int xSpawn, ySpawn, zSpawn;
    float rotSpawn;

    char name[64];
    char creator[64];
    long long createTime;
} Level;

typedef struct {
    int   size;
    int   capacity;
    AABB* aabbs;
} ArrayList_AABB;

ArrayList_AABB Level_getCubes(const Level* level, const AABB* boundingBox);

void  Level_init(Level* level);
// takes ownership of blocks
void  Level_setData(Level* level, int w, int d, int h, byte* blocks);
void  Level_destroy(Level* level);
void  Level_findSpawn(Level* level);
void  Level_setSpawnPos(Level* level, int x, int y, int z, float rot);

bool  Level_isSolidTile(const Level* level, int x, int y, int z);
bool  Level_isLightBlocker(const Level* level, int x, int y, int z);

void  calcLightDepths(Level* level, int x0, int z0, int x1, int z1);

bool  Level_load(Level* level, const char* path);
bool  Level_loadLegacy(Level* level, const char* path);
void  Level_save(const Level* level, const char* path);
// quitting before a level arrived from the server saves a null level
void  Level_saveNull(const char* path);

bool  level_setTile(Level* level, int x, int y, int z, int type);
bool  Level_setTileNoNeighborChange(Level* level, int x, int y, int z, int type);
// the same without the network mode check, for the server's changes and our own clicks
bool  Level_netSetTile(Level* level, int x, int y, int z, int type);
bool  Level_netSetTileNoNeighborChange(Level* level, int x, int y, int z, int type);
void  Level_updateNeighborsAt(Level* level, int x, int y, int z, int type);
bool  Level_isWater(const Level* level, int x, int y, int z);
bool  Level_setTileNoUpdate(Level* level, int x, int y, int z, int type);
void  Level_swap(Level* level, int x0, int y0, int z0, int x1, int y1, int z1);
int   Level_getTile(const Level* level, int x, int y, int z);

bool  Level_isLit(const Level* level, int x, int y, int z);
float Level_getBrightness(const Level* level, int x, int y, int z);
float Level_getGroundLevel(const Level* level);
float Level_getWaterLevel(const Level* level);
int   Level_getHighestTile(const Level* level, int x, int z);
bool  Level_isSolid(const Level* level, float x, float y, float z, float r);
// no entity intersects the box
bool  Level_isFree(const Level* level, const AABB* box);

bool  Level_containsAnyLiquid(const Level* level, const AABB* box);
bool  Level_containsLiquid(const Level* level, const AABB* box, int liquidId);

void  Level_addToTickNextTick(Level* level, int x, int y, int z, int id);
void  Level_addEntity(Level* level, const struct Zombie* z);
void  Level_addPlayer(Level* level, struct NetworkPlayer* p);
void  Level_removePlayer(Level* level, struct NetworkPlayer* p);
void  Level_tickEntities(Level* level);
void  Level_onTick(Level* level);

#endif  // LEVEL_H
