// level/level.c

#include "level.h"
#include "level_renderer.h"
#include "tile/tile.h"
#include "levelgen/level_gen.h"

#include <zlib.h>

bool Save_loadLevel(Level* level, const char* path);
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

// the light column array starts filled with depth before the first calcLightDepths
static int* allocLightDepths(int width, int height, int depth) {
    int* depths = (int*)malloc((size_t)width * height * sizeof(int));
    if (depths) {
        for (int i = 0; i < width * height; ++i) depths[i] = depth;
    }
    return depths;
}

void Level_init(Level* level, int width, int height, int depth) {
    level->width = width;
    level->height = height;
    level->depth = depth;
    level->renderer = NULL;
    level->unprocessed = 0;
    level->xSpawn = level->ySpawn = level->zSpawn = 0;
    level->rotSpawn = 0.0f;
    JRandom_initRandom(&level->random);
    level->tickRandom = (unsigned int)JRandom_nextInt(&level->random);
    level->tickCount = 0;
    level->tickList = NULL;
    level->tickListSize = 0;
    level->tickListCapacity = 0;
    level->networkMode = false;
    level->player = NULL; // set by Level_setPlayer
    level->skyColor = 0x99CCFF;
    level->fogColor = 0xFFFFFF;
    level->cloudColor = 0xFFFFFF;
    level->waterLevel = depth / 2;

    level->blocks = (byte*)malloc((size_t)width * height * depth);
    level->lightDepths = allocLightDepths(width, height, depth);
    if (!level->blocks || !level->lightDepths) {
        fprintf(stderr, "Failed to allocate level memory\n");
        exit(EXIT_FAILURE);
    }

    bool mapLoaded = Save_loadLevel(level, "level.dat");
    if (!mapLoaded) {
        Level_generateMap(level);
    }

    calcLightDepths(level, 0, 0, level->width, level->height);
    if (level->xSpawn == 0 && level->ySpawn == 0 && level->zSpawn == 0) Level_findSpawn(level);
}

// Minecraft.generateNewLevel, the size can change
void Level_resize(Level* level, int width, int height, int depth) {
    LevelRenderer* renderer = level->renderer;
    level->renderer = NULL; // calcLightDepths below must not touch the old renderer

    free(level->blocks);
    free(level->lightDepths);

    level->width = width;
    level->height = height;
    level->depth = depth;
    level->waterLevel = depth / 2;
    level->unprocessed = 0;
    level->xSpawn = level->ySpawn = level->zSpawn = 0;
    level->rotSpawn = 0.0f;
    free(level->tickList);
    level->tickList = NULL;
    level->tickListSize = level->tickListCapacity = 0;
    level->networkMode = false;
    JRandom_initRandom(&level->random);
    level->tickRandom = (unsigned int)JRandom_nextInt(&level->random);

    level->blocks = (byte*)malloc((size_t)width * height * depth);
    level->lightDepths = allocLightDepths(width, height, depth);
    if (!level->blocks || !level->lightDepths) {
        fprintf(stderr, "Failed to allocate level memory\n");
        exit(EXIT_FAILURE);
    }

    Level_generateMap(level);
    calcLightDepths(level, 0, 0, width, height);

    level->renderer = renderer;
}

// a level from a server, the spawn comes later in its own packet
void Level_setDataFromNetwork(Level* level, int width, int height, int depth, const byte* blocks) {
    LevelRenderer* renderer = level->renderer;
    level->renderer = NULL;

    free(level->blocks);
    free(level->lightDepths);

    level->width = width;
    level->height = height;
    level->depth = depth;
    level->waterLevel = depth / 2;
    level->unprocessed = 0;
    level->xSpawn = level->ySpawn = level->zSpawn = 0;
    level->rotSpawn = 0.0f;
    free(level->tickList);
    level->tickList = NULL;
    level->tickListSize = level->tickListCapacity = 0;
    level->networkMode = true;

    size_t total = (size_t)width * height * depth;
    level->blocks = (byte*)malloc(total);
    level->lightDepths = allocLightDepths(width, height, depth);
    if (!level->blocks || !level->lightDepths) {
        fprintf(stderr, "Failed to allocate level memory\n");
        exit(EXIT_FAILURE);
    }
    memcpy(level->blocks, blocks, total);

    calcLightDepths(level, 0, 0, width, height);

    level->renderer = renderer;
}

void calcLightDepths(Level* level, int minX, int minZ, int maxX, int maxZ) {
    for (int x = minX; x < minX + maxX; x++) {
        for (int z = minZ; z < minZ + maxZ; z++) {
            int prev = level->lightDepths[x + z * level->width];
            int d = level->depth - 1;
            while (d > 0 && !Level_isLightBlocker(level, x, d, z)) d--;
            level->lightDepths[x + z * level->width] = d + 1;

            if (prev != d && level->renderer) {
                int ylMin = prev < d ? prev : d;
                int ylMax = prev > d ? prev : d;
                levelRenderer_lightColumnChanged(level->renderer, x, z, ylMin, ylMax);
            }
        }
    }
}

void Level_generateMap(Level* level) {
    LevelGen_generateMap(level);
}

bool Level_isLightBlocker(const Level* level, int x, int y, int z) {
    int id = Level_getTile(level, x, y, z);
    const Tile* t = (id >= 0 && id < 256) ? gTiles[id] : NULL;
    return (t && t->blocksLight(t)) ? true : false;
}

bool Level_isTile(const Level* level, int x, int y, int z) {
    return Level_getTile(level, x, y, z) > 0;
}

bool Level_isSolidTile(const Level* level, int x, int y, int z) {
    int id = Level_getTile(level, x, y, z);
    const Tile* t = (id >= 0 && id < 256) ? gTiles[id] : NULL;
    return (t && t->isSolid(t)) ? true : false;
}

bool Level_containsAnyLiquid(const Level* level, const AABB* box) {
    int x0 = (int)box->minX, x1 = (int)box->maxX + 1;
    int y0 = (int)box->minY, y1 = (int)box->maxY + 1;
    int z0 = (int)box->minZ, z1 = (int)box->maxZ + 1;
    if (box->minX < 0.0f) --x0;
    if (box->minY < 0.0f) --y0;
    if (box->minZ < 0.0f) --z0;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (z0 < 0) z0 = 0;
    if (x1 > level->width)  x1 = level->width;
    if (y1 > level->depth)  y1 = level->depth;
    if (z1 > level->height) z1 = level->height;

    for (int x = x0; x < x1; ++x)
        for (int y = y0; y < y1; ++y)
            for (int z = z0; z < z1; ++z) {
                int id = Level_getTile(level, x, y, z);
                const Tile* t = (id >= 0 && id < 256) ? gTiles[id] : NULL;
                if (t && t->liquidType > LIQUID_NONE) return true;
            }
    return false;
}

bool Level_containsLiquid(const Level* level, const AABB* box, int liquidId) {
    int x0 = (int)box->minX, x1 = (int)box->maxX + 1;
    int y0 = (int)box->minY, y1 = (int)box->maxY + 1;
    int z0 = (int)box->minZ, z1 = (int)box->maxZ + 1;
    if (box->minX < 0.0f) --x0;
    if (box->minY < 0.0f) --y0;
    if (box->minZ < 0.0f) --z0;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (z0 < 0) z0 = 0;
    if (x1 > level->width)  x1 = level->width;
    if (y1 > level->depth)  y1 = level->depth;
    if (z1 > level->height) z1 = level->height;

    for (int x = x0; x < x1; ++x)
        for (int y = y0; y < y1; ++y)
            for (int z = z0; z < z1; ++z) {
                int id = Level_getTile(level, x, y, z);
                const Tile* t = (id >= 0 && id < 256) ? gTiles[id] : NULL;
                if (t && t->liquidType == liquidId) return true;
            }
    return false;
}

void Level_destroy(Level* level) {
    free(level->blocks);
    free(level->lightDepths);
    free(level->tickList);
}

ArrayList_AABB Level_getCubes(const Level* level, const AABB* aabb) {
    ArrayList_AABB out = { .size = 0, .capacity = 16 };
    out.aabbs = (AABB*)malloc((size_t)out.capacity * sizeof(AABB));

    int x0 = (int)aabb->minX, x1 = (int)aabb->maxX + 1;
    int y0 = (int)aabb->minY, y1 = (int)aabb->maxY + 1;
    int z0 = (int)aabb->minZ, z1 = (int)aabb->maxZ + 1;
    if (aabb->minX < 0.0f) --x0;
    if (aabb->minY < 0.0f) --y0;
    if (aabb->minZ < 0.0f) --z0;

    for (int x = x0; x < x1; ++x)
    for (int y = y0; y < y1; ++y)
    for (int z = z0; z < z1; ++z) {
        AABB box;
        int haveBox = 0;

        if (x >= 0 && y >= 0 && z >= 0 && x < level->width && y < level->depth && z < level->height) {
            int id = Level_getTile(level, x, y, z);
            const Tile* t = (id >= 0 && id < 256) ? gTiles[id] : NULL;
            if (t && t->getAABB && t->getAABB(t, x, y, z, &box)) haveBox = 1;
        } else if (x < 0 || y < 0 || z < 0 || x >= level->width || z >= level->height) {
            // outside the map sideways or below the floor is an unbreakable wall, above the map is open
            box = AABB_create(x, y, z, x+1, y+1, z+1);
            haveBox = 1;
        }

        if (haveBox) {
            if (out.size == out.capacity) {
                out.capacity *= 2;
                out.aabbs = (AABB*)realloc(out.aabbs, (size_t)out.capacity * sizeof(AABB));
            }
            out.aabbs[out.size++] = box;
        }
    }
    return out;
}

// takes the blocks, like Level.setData
void Level_setBlocks(Level* level, int width, int height, int depth, byte* blocks) {
    free(level->blocks);
    free(level->lightDepths);
    level->width = width;
    level->height = height;
    level->depth = depth;
    level->waterLevel = depth / 2;
    level->blocks = blocks;
    level->lightDepths = allocLightDepths(width, height, depth);
    if (!level->lightDepths) {
        fprintf(stderr, "Failed to allocate level memory\n");
        exit(EXIT_FAILURE);
    }
    if (level->renderer) levelRenderer_allChanged(level, level->renderer);
}

static void notifyNeighborChanged(Level* level, int x, int y, int z, int type) {
    if (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height) return;
    int id = Level_getTile(level, x, y, z);
    const Tile* t = (id >= 0 && id < 256) ? gTiles[id] : NULL;
    if (t && t->neighborChanged) t->neighborChanged(t, level, x, y, z, type);
}

void Level_updateNeighborsAt(Level* level, int x, int y, int z, int type) {
    notifyNeighborChanged(level, x - 1, y, z, type);
    notifyNeighborChanged(level, x + 1, y, z, type);
    notifyNeighborChanged(level, x, y - 1, z, type);
    notifyNeighborChanged(level, x, y + 1, z, type);
    notifyNeighborChanged(level, x, y, z - 1, type);
    notifyNeighborChanged(level, x, y, z + 1, type);
}

// emptying a block on the map edge between ground and water level refills it with water
bool Level_netSetTileNoNeighborChange(Level* level, int x, int y, int z, int type) {
    if (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height) return false;
    int index = (y * level->height + z) * level->width + x;
    if (type == level->blocks[index]) return false;
    if (type == 0 && (x == 0 || z == 0 || x == level->width - 1 || z == level->height - 1) &&
        (float)y >= Level_getGroundLevel(level) && (float)y < Level_getWaterLevel(level)) {
        type = TILE_WATER.id;
    }
    int oldType = level->blocks[index];
    level->blocks[index] = (byte)type;
    if (oldType != 0) {
        const Tile* oldTile = gTiles[oldType];
        if (oldTile && oldTile->onRemoved) oldTile->onRemoved(oldTile, level, x, y, z);
    }
    if (type != 0) {
        const Tile* newTile = gTiles[type];
        if (newTile && newTile->onPlace) newTile->onPlace(newTile, level, x, y, z);
    }
    calcLightDepths(level, x, z, 1, 1);
    if (level->renderer) levelRenderer_tileChanged(level->renderer, x, y, z);
    return true;
}

bool Level_setTileNoNeighborChange(Level* level, int x, int y, int z, int type) {
    if (level->networkMode) return false;
    return Level_netSetTileNoNeighborChange(level, x, y, z, type);
}

bool level_setTile(Level* level, int x, int y, int z, int type) {
    if (level->networkMode) return false;
    if (Level_setTileNoNeighborChange(level, x, y, z, type)) {
        Level_updateNeighborsAt(level, x, y, z, type);
        return true;
    }
    return false;
}

bool Level_netSetTile(Level* level, int x, int y, int z, int type) {
    if (Level_netSetTileNoNeighborChange(level, x, y, z, type)) {
        Level_updateNeighborsAt(level, x, y, z, type);
        return true;
    }
    return false;
}

// no neighbor update, no light, no renderer update
bool Level_setTileNoUpdate(Level* level, int x, int y, int z, int type) {
    if (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height) return false;
    int index = (y * level->height + z) * level->width + x;
    if (level->blocks[index] == (byte)type) return false;
    level->blocks[index] = (byte)type;
    return true;
}

int Level_getTile(const Level* level, int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height)
        return 0;
    int index = (y * level->height + z) * level->width + x;
    return level->blocks[index];
}

// Level.clip, the start voxel is never tested and it gives up after 21 steps
// moves the start point along the ray
bool Level_clipHit(const Level* level, float* sx, float* sy, float* sz, float ex, float ey, float ez,
                   int* outX, int* outY, int* outZ, int* outFace, float* hitX, float* hitY, float* hitZ) {
    if (isnan(*sx) || isnan(*sy) || isnan(*sz)) return false;
    if (isnan(ex) || isnan(ey) || isnan(ez)) return false;
    int endX = (int)floor(ex), endY = (int)floor(ey), endZ = (int)floor(ez);
    int x = (int)floor(*sx), y = (int)floor(*sy), z = (int)floor(*sz);
    int steps = 20;
    while (steps-- >= 0) {
        if (isnan(*sx) || isnan(*sy) || isnan(*sz)) return false;
        if (x == endX && y == endY && z == endZ) return false;
        float bx = 999.0f, by = 999.0f, bz = 999.0f;
        if (endX > x) bx = (float)x + 1.0f;
        if (endX < x) bx = (float)x;
        if (endY > y) by = (float)y + 1.0f;
        if (endY < y) by = (float)y;
        if (endZ > z) bz = (float)z + 1.0f;
        if (endZ < z) bz = (float)z;
        float tx = 999.0f, ty = 999.0f, tz = 999.0f;
        float dx = ex - *sx, dy = ey - *sy, dz = ez - *sz;
        if (bx != 999.0f) tx = (bx - *sx) / dx;
        if (by != 999.0f) ty = (by - *sy) / dy;
        if (bz != 999.0f) tz = (bz - *sz) / dz;
        int face;
        if (tx < ty && tx < tz) {
            face = endX > x ? 4 : 5;
            *sx = bx;
            *sy += dy * tx;
            *sz += dz * tx;
        } else if (ty < tz) {
            face = endY > y ? 0 : 1;
            *sx += dx * ty;
            *sy = by;
            *sz += dz * ty;
        } else {
            face = endZ > z ? 2 : 3;
            *sx += dx * tz;
            *sy += dy * tz;
            *sz = bz;
        }
        float hx = (float)floor(*sx), hy, hz;
        x = (int)hx;
        if (face == 5) { x--; hx -= 1.0f; }
        hy = (float)floor(*sy);
        y = (int)hy;
        if (face == 1) { y--; hy -= 1.0f; }
        hz = (float)floor(*sz);
        z = (int)hz;
        if (face == 3) { z--; hz -= 1.0f; }
        int id = Level_getTile(level, x, y, z);
        if (id <= 0 || !gTiles[id] || gTiles[id]->liquidType != LIQUID_NONE) continue;
        *outX = x; *outY = y; *outZ = z; *outFace = face;
        *hitX = hx; *hitY = hy; *hitZ = hz;
        return true;
    }
    return false;
}

bool Level_clip(const Level* level, float x1, float y1, float z1, float x2, float y2, float z2) {
    int x, y, z, face;
    float hx, hy, hz;
    return Level_clipHit(level, &x1, &y1, &z1, x2, y2, z2, &x, &y, &z, &face, &hx, &hy, &hz);
}

bool Level_maybeGrowTree(Level* level, int x, int y, int z) {
    int trunkHeight = JRandom_nextIntN(&level->random, 3) + 4;

    for (int ny = y; ny <= y + 1 + trunkHeight; ny++) {
        int radius = 1;
        if (ny == y) radius = 0;
        if (ny >= y + 1 + trunkHeight - 2) radius = 2;
        for (int nx = x - radius; nx <= x + radius; nx++) {
            for (int nz = z - radius; nz <= z + radius; nz++) {
                if (nx < 0 || ny < 0 || nz < 0 || nx >= level->width || ny >= level->depth || nz >= level->height)
                    return false;
                if (Level_getTile(level, nx, ny, nz) != 0) return false;
            }
        }
    }

    if (Level_getTile(level, x, y - 1, z) != TILE_GRASS.id) return false;
    if (y >= level->depth - trunkHeight - 1) return false;

    level_setTile(level, x, y - 1, z, TILE_DIRT.id);

    // the outer corners are dropped half the time, always on the top layer
    for (int ly = y - 3 + trunkHeight; ly <= y + trunkHeight; ly++) {
        int fromTop = ly - (y + trunkHeight); // -3..0, 0 = the very top layer
        int radius = 1 - fromTop / 2;
        for (int lx = x - radius; lx <= x + radius; lx++) {
            int dx = lx - x;
            for (int lz = z - radius; lz <= z + radius; lz++) {
                int dz = lz - z;
                if (abs(dx) == radius && abs(dz) == radius && (JRandom_nextIntN(&level->random, 2) == 0 || fromTop == 0)) continue;
                level_setTile(level, lx, ly, lz, TILE_LEAVES.id);
            }
        }
    }
    for (int ly = 0; ly < trunkHeight; ly++) {
        level_setTile(level, x, y + ly, z, TILE_LOG.id);
    }
    return true;
}

AABB Level_getTilePickAABB(const Level* level, int x, int y, int z) {
    (void)level;
    return AABB_create(x, y, z, x+1, y+1, z+1);
}

bool Level_isLit(const Level* level, int x, int y, int z) {
    return (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height) ||
           (y >= level->lightDepths[x + z * level->width]);
}

float Level_getBrightness(const Level* level, int x, int y, int z) {
    return Level_isLit(level, x, y, z) ? 1.0f : 0.6f;
}

int Level_getHighestTile(const Level* level, int x, int z) {
    int i = level->depth;
    while (i > 0) {
        int id = Level_getTile(level, x, i - 1, z);
        const Tile* t = (id >= 0 && id < 256) ? gTiles[id] : NULL;
        bool airOrLiquid = (id == 0) || (t && t->liquidType != LIQUID_NONE);
        if (!airOrLiquid) break;
        i--;
    }
    return i;
}

float Level_getGroundLevel(const Level* level) {
    return Level_getWaterLevel(level) - 2.0f;
}

float Level_getWaterLevel(const Level* level) {
    return (float)level->waterLevel;
}

// gives up with y -100 after 10000 tries
void Level_findSpawn(Level* level) {
    JRandom random;
    JRandom_initRandom(&random);
    int tries = 0;
    while (1) {
        int x = JRandom_nextIntN(&random, level->width / 2) + level->width / 4;
        int z = JRandom_nextIntN(&random, level->height / 2) + level->height / 4;
        int y = Level_getHighestTile(level, x, z) + 1;
        if (++tries == 10000) {
            level->xSpawn = x;
            level->ySpawn = -100;
            level->zSpawn = z;
            return;
        }
        if ((float)y > Level_getWaterLevel(level)) {
            level->xSpawn = x;
            level->ySpawn = y;
            level->zSpawn = z;
            return;
        }
    }
}

void Level_setSpawnPos(Level* level, int x, int y, int z, float rot) {
    level->xSpawn = x;
    level->ySpawn = y;
    level->zSpawn = z;
    level->rotSpawn = rot;
}

void Level_swap(Level* level, int x1, int y1, int z1, int x2, int y2, int z2) {
    if (level->networkMode) return;
    int a = Level_getTile(level, x1, y1, z1);
    int b = Level_getTile(level, x2, y2, z2);
    Level_setTileNoNeighborChange(level, x1, y1, z1, b);
    Level_setTileNoNeighborChange(level, x2, y2, z2, a);
    Level_updateNeighborsAt(level, x1, y1, z1, b);
    Level_updateNeighborsAt(level, x2, y2, z2, a);
}

static void pushTickEntry(Level* level, TickEntry entry) {
    if (level->tickListSize == level->tickListCapacity) {
        level->tickListCapacity = level->tickListCapacity ? level->tickListCapacity * 2 : 16;
        level->tickList = (TickEntry*)realloc(level->tickList, (size_t)level->tickListCapacity * sizeof(TickEntry));
    }
    level->tickList[level->tickListSize++] = entry;
}

void Level_addToTickNextTick(Level* level, int x, int y, int z, int tileId) {
    if (level->networkMode) return;
    TickEntry e = { x, y, z, tileId, 0 };
    if (tileId > 0 && tileId < 256 && gTiles[tileId]) e.delay = gTiles[tileId]->tickDelay;
    pushTickEntry(level, e);
}

void Level_onTick(Level* level) {
    level->tickCount++;

    if (level->tickCount % 5 == 0 && level->tickListSize > 0) {
        int n = level->tickListSize;
        // one pass over what was queued so far, delayed entries go back to the end
        for (int i = 0; i < n; ++i) {
            TickEntry e = level->tickList[i];
            if (e.delay > 0) {
                e.delay--;
                pushTickEntry(level, e);
                continue;
            }
            if (e.x < 0 || e.y < 0 || e.z < 0 || e.x >= level->width || e.y >= level->depth || e.z >= level->height) continue;
            int current = Level_getTile(level, e.x, e.y, e.z);
            if (current == e.tileId && current > 0) {
                const Tile* t = gTiles[current];
                if (t && t->onTick) t->onTick(t, level, e.x, e.y, e.z);
            }
        }
        int remaining = level->tickListSize - n;
        memmove(level->tickList, level->tickList + n, (size_t)remaining * sizeof(TickEntry));
        level->tickListSize = remaining;
    }

    level->unprocessed += level->width * level->height * level->depth;
    int ticks = level->unprocessed / 200;
    level->unprocessed -= ticks * 200;

    // the d*3+1013904223 generator picks the random tile, one draw split into x z y
    int bitsX = 1; while ((1 << bitsX) < level->width)  bitsX++;
    int bitsZ = 1; while ((1 << bitsZ) < level->height) bitsZ++;
    int maskX = level->width  - 1;
    int maskZ = level->height - 1;
    int maskY = level->depth  - 1;

    for (int i = 0; i < ticks; ++i) {
        level->tickRandom = level->tickRandom * 3 + 1013904223;
        int shifted = (int)level->tickRandom >> 2;
        int x = shifted & maskX;
        int z = (shifted >> bitsX) & maskZ;
        int y = (shifted >> (bitsX + bitsZ)) & maskY;
        int id = Level_getTile(level, x, y, z);
        const Tile* t = (id >= 0 && id < 256) ? gTiles[id] : NULL;
        if (t && t->ticksRandomly && t->onTick) t->onTick(t, level, x, y, z);
    }
}

extern void Minecraft_hurtEntitiesInExplosion(Level* level, Entity* source, float x, float y, float z, float radius);

// every block with its center in the radius goes, each with a 30% chance of its drops
// then Minecraft_hurtEntitiesInExplosion hurts the entities
void Level_explode(Level* level, Entity* source, float x, float y, float z, float radius) {
    int x0 = (int)(x - radius - 1.0f);
    int x1 = (int)(x + radius + 1.0f);
    int y0 = (int)(y - radius - 1.0f);
    int y1 = (int)(y + radius + 1.0f);
    int z0 = (int)(z - radius - 1.0f);
    int z1 = (int)(z + radius + 1.0f);

    for (int bx = x0; bx < x1; ++bx) {
        for (int by = y1 - 1; by >= y0; --by) {
            for (int bz = z0; bz < z1; ++bz) {
                if (bx < 0 || by < 0 || bz < 0 || bx >= level->width || by >= level->depth || bz >= level->height) continue;
                float dx = (float)bx + 0.5f - x;
                float dy = (float)by + 0.5f - y;
                float dz = (float)bz + 0.5f - z;
                if (!(dx * dx + dy * dy + dz * dz < radius * radius)) continue;
                int id = Level_getTile(level, bx, by, bz);
                if (id <= 0) continue;
                const Tile* t = (id < 256) ? gTiles[id] : NULL;
                if (t) Tile_dropItemsChance(t, level, bx, by, bz, 0.3f);
                level_setTile(level, bx, by, bz, 0);
            }
        }
    }

    Minecraft_hurtEntitiesInExplosion(level, source, x, y, z, radius);
}