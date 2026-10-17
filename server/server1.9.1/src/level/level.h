// level/level.h: the server's Level, blocks, light columns, tile ticks and the change listener

#ifndef LEVEL_H
#define LEVEL_H

#include "../jrandom.h"
#include "../jstring.h"
#include <stdbool.h>
#include <stdint.h>

// a scheduled tile tick, delay is how many 5 tick rounds it still waits
typedef struct {
    int x, y, z;
    int id;
    int delay;
} NextTick;

// the BlockMap only travels with the save, the server never puts an entity in it
typedef struct {
    int width, depth, height;
    // array lengths of the saved lists, all, tmp and every grid cell
    int allCapacity, tmpCapacity;
    int cells;
    int* cellCapacity;
    int slot[2][3];
} BlockMap;

typedef struct Level {
    int width, height, depth;
    int8_t* blocks;
    JStr* name;
    JStr* creator;
    int64_t createTime;
    int xSpawn, ySpawn, zSpawn;
    float rotSpawn;
    BlockMap* blockMap;
    bool networkMode;
    bool creativeMode;
    int waterLevel;
    int skyColor, fogColor, cloudColor;
    int unprocessed;
    int tickCount;

    // the server once it's listening, it sends every change on
    void (*listener)(void* ctx, int x, int y, int z);
    void* listenerCtx;

    int* lightDepths;
    JRandom random;
    int32_t randValue;
    NextTick* ticks;
    int tickHead, tickSize, tickCap;
} Level;

Level* Level_create(void);
BlockMap* BlockMap_create(int width, int depth, int height);
void Level_setData(Level* level, int width, int depth, int height, int8_t* blocks);
void Level_initTransient(Level* level);
void Level_findSpawn(Level* level);
void Level_calcLightDepths(Level* level, int x0, int z0, int w, int h);
bool Level_isLightBlocker(Level* level, int x, int y, int z);
void Level_swap(Level* level, int x0, int y0, int z0, int x1, int y1, int z1);
bool Level_setTileNoNeighborChange(Level* level, int x, int y, int z, int type);
bool Level_setTile(Level* level, int x, int y, int z, int type);
void Level_updateNeighborsAt(Level* level, int x, int y, int z, int type);
bool Level_isWater(const Level* level, int x, int y, int z);
bool Level_setTileNoUpdate(Level* level, int x, int y, int z, int type);
bool Level_isLit(const Level* level, int x, int y, int z);
int  Level_getTile(const Level* level, int x, int y, int z);
void Level_tick(Level* level);
float Level_getGroundLevel(const Level* level);
float Level_getWaterLevel(const Level* level);
void Level_addToTickNextTick(Level* level, int x, int y, int z, int id);
int  Level_getHighestTile(Level* level, int x, int z);
void Level_setSpawnPos(Level* level, int x, int y, int z, float rot);
bool Level_maybeGrowTree(Level* level, int x, int y, int z);
int8_t* Level_copyBlocks(const Level* level);

#endif
