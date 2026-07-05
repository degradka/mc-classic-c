// level/level.h: world storage, light columns, ticking and box queries

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

typedef struct Level {
    int   width, height, depth;
    byte* blocks;
    int*  lightDepths;
    LevelRenderer* renderer;
    JRandom random;
    int randValue;
    int unprocessed;

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

bool  Level_isSolidTile(const Level* level, int x, int y, int z);
bool  Level_isLightBlocker(const Level* level, int x, int y, int z);

void  calcLightDepths(Level* level, int x0, int z0, int x1, int z1);

bool  Level_load(Level* level, const char* path);
bool  Level_loadLegacy(Level* level, const char* path);
void  Level_save(const Level* level, const char* path);

bool  level_setTile(Level* level, int x, int y, int z, int type);
bool  Level_setTileNoUpdate(Level* level, int x, int y, int z, int type);
int   Level_getTile(const Level* level, int x, int y, int z);

bool  Level_isLit(const Level* level, int x, int y, int z);

bool  Level_containsAnyLiquid(const Level* level, const AABB* box);
bool  Level_containsLiquid(const Level* level, const AABB* box, int liquidId);

void  Level_onTick(Level* level);

#endif  // LEVEL_H
