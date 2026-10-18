// level/level.c

#include "level.h"
#include "tile/tile.h"
#include "../jexc.h"
#include <stdlib.h>
#include <string.h>

Level* Level_create(void) {
    Level* level = (Level*)calloc(1, sizeof *level);
    JRandom_initRandom(&level->random);
    level->randValue = JRandom_nextInt(&level->random);
    return level;
}

BlockMap* BlockMap_create(int width, int depth, int height) {
    BlockMap* map = (BlockMap*)calloc(1, sizeof *map);
    map->width = width / 16;
    map->depth = depth / 16;
    map->height = height / 16;
    if (map->width == 0) map->width = 1;
    if (map->depth == 0) map->depth = 1;
    if (map->height == 0) map->height = 1;
    map->allCapacity = map->tmpCapacity = 10;
    map->cells = map->width * map->depth * map->height;
    map->cellCapacity = (int*)malloc((size_t)map->cells * sizeof(int));
    for (int i = 0; i < map->cells; ++i) map->cellCapacity[i] = 10;
    return map;
}

static void resetLight(Level* level) {
    free(level->lightDepths);
    level->lightDepths = (int*)malloc((size_t)level->width * level->height * sizeof(int));
    for (int i = 0; i < level->width * level->height; ++i) level->lightDepths[i] = level->depth;
    Level_calcLightDepths(level, 0, 0, level->width, level->height);
}

void Level_setData(Level* level, int width, int depth, int height, int8_t* blocks) {
    level->width = width;
    level->height = height;
    level->depth = depth;
    free(level->blocks);
    level->blocks = blocks;
    resetLight(level);
    level->tickHead = level->tickSize = 0;
    Level_findSpawn(level);
    Level_initTransient(level);
}

void Level_initTransient(Level* level) {
    resetLight(level);
    JRandom_initRandom(&level->random);
    level->randValue = JRandom_nextInt(&level->random);
    level->tickHead = level->tickSize = 0;
    if (level->waterLevel == 0) level->waterLevel = level->depth / 2;
    if (level->skyColor == 0) level->skyColor = 0x99CCFF;
    if (level->fogColor == 0) level->fogColor = 0xFFFFFF;
    if (level->cloudColor == 0) level->cloudColor = 0xFFFFFF;
    if (level->xSpawn == 0 && level->ySpawn == 0 && level->zSpawn == 0) Level_findSpawn(level);
    if (!level->blockMap) level->blockMap = BlockMap_create(level->width, level->depth, level->height);
}

void Level_findSpawn(Level* level) {
    JRandom random;
    JRandom_initRandom(&random);
    int x, y, z, tries = 0;
    do {
        x = JRandom_nextIntN(&random, level->width / 2) + level->width / 4;
        z = JRandom_nextIntN(&random, level->height / 2) + level->height / 4;
        y = Level_getHighestTile(level, x, z) + 1;
        if (++tries == 10000) {
            level->xSpawn = x;
            level->ySpawn = -100;
            level->zSpawn = z;
            return;
        }
    } while ((float)y <= Level_getWaterLevel(level));
    level->xSpawn = x;
    level->ySpawn = y;
    level->zSpawn = z;
}

void Level_calcLightDepths(Level* level, int x0, int z0, int w, int h) {
    for (int x = x0; x < x0 + w; ++x) {
        for (int z = z0; z < z0 + h; ++z) {
            int y = level->depth - 1;
            while (y > 0 && !Level_isLightBlocker(level, x, y, z)) --y;
            level->lightDepths[x + z * level->width] = y;
        }
    }
}

bool Level_isLightBlocker(Level* level, int x, int y, int z) {
    const Tile* t = Tile_get(Level_getTile(level, x, y, z));
    return t ? t->blocksLight : false;
}

static bool inBounds(const Level* level, int x, int y, int z) {
    return x >= 0 && y >= 0 && z >= 0 && x < level->width && y < level->depth && z < level->height;
}

static int indexOf(const Level* level, int x, int y, int z) {
    return (y * level->height + z) * level->width + x;
}

// tells one neighbor, the tile there decides what to do
static void neighborChanged(Level* level, int x, int y, int z, int type) {
    if (!inBounds(level, x, y, z)) return;
    const Tile* t = Tile_get(level->blocks[indexOf(level, x, y, z)]);
    if (t && t->neighborChanged) t->neighborChanged(t, level, x, y, z, type);
}

void Level_updateNeighborsAt(Level* level, int x, int y, int z, int type) {
    neighborChanged(level, x - 1, y, z, type);
    neighborChanged(level, x + 1, y, z, type);
    neighborChanged(level, x, y - 1, z, type);
    neighborChanged(level, x, y + 1, z, type);
    neighborChanged(level, x, y, z - 1, type);
    neighborChanged(level, x, y, z + 1, type);
}

void Level_swap(Level* level, int x0, int y0, int z0, int x1, int y1, int z1) {
    if (level->networkMode) return;
    int a = Level_getTile(level, x0, y0, z0);
    int b = Level_getTile(level, x1, y1, z1);
    Level_setTileNoNeighborChange(level, x0, y0, z0, b);
    Level_setTileNoNeighborChange(level, x1, y1, z1, a);
    Level_updateNeighborsAt(level, x0, y0, z0, b);
    Level_updateNeighborsAt(level, x1, y1, z1, a);
}

bool Level_setTileNoNeighborChange(Level* level, int x, int y, int z, int type) {
    if (level->networkMode) return false;
    if (!inBounds(level, x, y, z)) return false;
    int i = indexOf(level, x, y, z);
    if (type == level->blocks[i]) return false;
    // air on the map's edge between the ground and the water level turns into water
    if (type == 0 && (x == 0 || z == 0 || x == level->width - 1 || z == level->height - 1) &&
        (float)y >= Level_getGroundLevel(level) && (float)y < Level_getWaterLevel(level)) {
        type = TILE_WATER;
    }
    int old = level->blocks[i];
    level->blocks[i] = (int8_t)type;
    if (old != 0) {
        const Tile* t = Tile_req(old);
        if (t->onRemoved) t->onRemoved(t, level, x, y, z);
    }
    if (type != 0) {
        const Tile* t = Tile_req(type);
        if (t->onAdded) t->onAdded(t, level, x, y, z);
    }
    Level_calcLightDepths(level, x, z, 1, 1);
    if (level->listener) level->listener(level->listenerCtx, x, y, z);
    return true;
}

bool Level_setTile(Level* level, int x, int y, int z, int type) {
    if (level->networkMode) return false;
    if (!Level_setTileNoNeighborChange(level, x, y, z, type)) return false;
    Level_updateNeighborsAt(level, x, y, z, type);
    return true;
}

bool Level_setTileNoUpdate(Level* level, int x, int y, int z, int type) {
    if (!inBounds(level, x, y, z)) return false;
    int i = indexOf(level, x, y, z);
    if (type == level->blocks[i]) return false;
    level->blocks[i] = (int8_t)type;
    return true;
}

bool Level_isLit(const Level* level, int x, int y, int z) {
    if (!inBounds(level, x, y, z)) return true;
    return y >= level->lightDepths[x + z * level->width];
}

int Level_getTile(const Level* level, int x, int y, int z) {
    if (!inBounds(level, x, y, z)) return 0;
    // unsigned here, the ticks and neighbor updates still read the signed byte
    return level->blocks[indexOf(level, x, y, z)] & 0xFF;
}

bool Level_isWater(const Level* level, int x, int y, int z) {
    int id = Level_getTile(level, x, y, z);
    return id > 0 && Tile_req(id)->liquid == LIQUID_WATER;
}

static void pushTick(Level* level, NextTick t) {
    if (level->tickHead + level->tickSize == level->tickCap) {
        if (level->tickHead > 0) {
            memmove(level->ticks, level->ticks + level->tickHead, (size_t)level->tickSize * sizeof(NextTick));
            level->tickHead = 0;
        }
        if (level->tickSize == level->tickCap) {
            level->tickCap = level->tickCap ? level->tickCap * 2 : 256;
            level->ticks = (NextTick*)realloc(level->ticks, (size_t)level->tickCap * sizeof(NextTick));
        }
    }
    level->ticks[level->tickHead + level->tickSize++] = t;
}

void Level_tick(Level* level) {
    ++level->tickCount;
    int xBits = 1, zBits = 1;
    while (1 << xBits < level->width) ++xBits;
    while (1 << zBits < level->height) ++zBits;
    int zMask = level->height - 1;
    int xMask = level->width - 1;
    int yMask = level->depth - 1;
    if (level->tickCount % 5 == 0) {
        int n = level->tickSize;
        for (int i = 0; i < n; ++i) {
            NextTick t = level->ticks[level->tickHead++];
            level->tickSize--;
            if (t.delay > 0) {
                --t.delay;
                pushTick(level, t);
                continue;
            }
            if (!inBounds(level, t.x, t.y, t.z)) continue;
            int id = level->blocks[indexOf(level, t.x, t.y, t.z)];
            if (id != t.id || id <= 0) continue;
            const Tile* tile = Tile_req(id);
            if (tile->tick) tile->tick(tile, level, t.x, t.y, t.z, &level->random);
        }
    }
    level->unprocessed += level->width * level->height * level->depth;
    int count = level->unprocessed / 200;
    level->unprocessed -= count * 200;
    for (int i = 0; i < count; ++i) {
        level->randValue = (int32_t)((uint32_t)level->randValue * 3u + 1013904223u);
        int r = level->randValue >> 2;
        int z = r >> (xBits & 31) & zMask;
        int x = r & xMask;
        int y = r >> ((xBits + zBits) & 31) & yMask;
        int id = level->blocks[(y * level->height + z) * level->width + x];
        if (id < 0) jthrowIndex(id);
        if (!Tile_shouldTick[id]) continue;
        const Tile* tile = Tile_req(id);
        if (tile->tick) tile->tick(tile, level, x, y, z, &level->random);
    }
}

float Level_getGroundLevel(const Level* level) {
    return Level_getWaterLevel(level) - 2.0f;
}

float Level_getWaterLevel(const Level* level) {
    return (float)level->waterLevel;
}

void Level_addToTickNextTick(Level* level, int x, int y, int z, int id) {
    if (level->networkMode) return;
    NextTick t = { x, y, z, id, 0 };
    if (id > 0) t.delay = Tile_req(id)->tickDelay;
    pushTick(level, t);
}

int Level_getHighestTile(Level* level, int x, int z) {
    int y = level->depth;
    for (;;) {
        int id = Level_getTile(level, x, y - 1, z);
        bool open = id == 0 || Tile_req(id)->liquid != LIQUID_NONE;
        if (!open || y <= 0) break;
        --y;
    }
    return y;
}

void Level_setSpawnPos(Level* level, int x, int y, int z, float rot) {
    level->xSpawn = x;
    level->ySpawn = y;
    level->zSpawn = z;
    level->rotSpawn = rot;
}

// needs grass under it and room for the whole tree
bool Level_maybeGrowTree(Level* level, int x, int y, int z) {
    int height = JRandom_nextIntN(&level->random, 3) + 4;
    bool fits = true;
    for (int yy = y; yy <= y + 1 + height; ++yy) {
        int r = 1;
        if (yy == y) r = 0;
        if (yy >= y + 1 + height - 2) r = 2;
        for (int xx = x - r; xx <= x + r && fits; ++xx) {
            for (int zz = z - r; zz <= z + r && fits; ++zz) {
                if (inBounds(level, xx, yy, zz)) {
                    if ((level->blocks[(yy * level->height + zz) * level->width + xx] & 0xFF) != 0) fits = false;
                    continue;
                }
                fits = false;
            }
        }
    }
    if (!fits) return false;
    int i = ((y - 1) * level->height + z) * level->width + x;
    if (i < 0) jthrowIndex(i);
    int below = level->blocks[i] & 0xFF;
    if (below != TILE_GRASS || y >= level->depth - height - 1) return false;
    Level_setTile(level, x, y - 1, z, TILE_DIRT);
    for (int yy = y - 3 + height; yy <= y + height; ++yy) {
        int dy = yy - (y + height);
        int r = 1 - dy / 2;
        for (int xx = x - r; xx <= x + r; ++xx) {
            int dx = xx - x;
            for (int zz = z - r; zz <= z + r; ++zz) {
                int dz = zz - z;
                if (abs(dx) == r && abs(dz) == r && (JRandom_nextIntN(&level->random, 2) == 0 || dy == 0)) continue;
                Level_setTile(level, xx, yy, zz, TILE_LEAVES);
            }
        }
    }
    for (int j = 0; j < height; ++j) Level_setTile(level, x, y + j, z, TILE_LOG);
    return true;
}

int8_t* Level_copyBlocks(const Level* level) {
    size_t n = (size_t)level->width * level->height * level->depth;
    int8_t* copy = (int8_t*)malloc(n);
    memcpy(copy, level->blocks, n);
    return copy;
}
