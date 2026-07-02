// level/level.c — world storage, lighting columns, IO, and solid-cube queries

#include "level.h"
#include "level_renderer.h"
#include "tile/tile.h"
#include "levelgen/level_gen.h"

#include <zlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

void Level_init(Level* level, int width, int height, int depth) {
    level->width = width;
    level->height = height;
    level->depth = depth;
    level->renderer = NULL;
    level->unprocessed = 0;

    level->blocks = (byte*)malloc((size_t)width * height * depth);
    level->lightDepths = (int*)malloc((size_t)width * height * sizeof(int));
    if (!level->blocks || !level->lightDepths) {
        fprintf(stderr, "Failed to allocate level memory\n");
        exit(EXIT_FAILURE);
    }

    bool mapLoaded = Level_load(level);
    if (!mapLoaded) {
        Level_generateMap(level);
    }

    calcLightDepths(level, 0, 0, width, height);
}

void calcLightDepths(Level* level, int minX, int minZ, int maxX, int maxZ) {
    for (int x = minX; x < minX + maxX; x++) {
        for (int z = minZ; z < minZ + maxZ; z++) {
            int prev = level->lightDepths[x + z * level->width];
            int d = level->depth - 1;
            while (d > 0 && !Level_isLightBlocker(level, x, d, z)) d--;
            level->lightDepths[x + z * level->width] = d;

            if (prev != d && level->renderer) {
                int ylMin = prev < d ? prev : d;
                int ylMax = prev > d ? prev : d;
                levelRenderer_lightColumnChanged(level->renderer, x, z, ylMin, ylMax);
            }
        }
    }
}

void Level_generateMap(Level* level) {
    // c0.0.13a replaced the old Perlin-hills generator with flat terrain +
    // carved caves + flood-filled lakes (see level_gen.h for why it's flat).
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
    int x0 = (int)box->minX, x1 = (int)(box->maxX + 1.0);
    int y0 = (int)box->minY, y1 = (int)(box->maxY + 1.0);
    int z0 = (int)box->minZ, z1 = (int)(box->maxZ + 1.0);

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
    int x0 = (int)box->minX, x1 = (int)(box->maxX + 1.0);
    int y0 = (int)box->minY, y1 = (int)(box->maxY + 1.0);
    int z0 = (int)box->minZ, z1 = (int)(box->maxZ + 1.0);

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
}

ArrayList_AABB Level_getCubes(const Level* level, const AABB* aabb) {
    ArrayList_AABB out = { .size = 0, .capacity = 16 };
    out.aabbs = (AABB*)malloc((size_t)out.capacity * sizeof(AABB));

    int x0 = (int)aabb->minX;
    int x1 = (int)(aabb->maxX + 1.0);
    int y0 = (int)aabb->minY;
    int y1 = (int)(aabb->maxY + 1.0);
    int z0 = (int)aabb->minZ;
    int z1 = (int)(aabb->maxZ + 1.0);

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
        if (!t) continue;
        // Classic tiles are axis-aligned unit cubes; keep the hook anyway:
        AABB box = AABB_create(x, y, z, x+1, y+1, z+1);
        if (t->isSolid && t->isSolid(t)) {
            if (out.size == out.capacity) {
                out.capacity *= 2;
                out.aabbs = (AABB*)realloc(out.aabbs, (size_t)out.capacity * sizeof(AABB));
            }
            out.aabbs[out.size++] = box;
        }
    }
    return out;
}

bool Level_load(Level* level) {
    gzFile f = gzopen("level.dat", "rb");
    if (!f) return false;

    size_t total = (size_t)level->width * level->height * level->depth;
    int readBytes = gzread(f, level->blocks, (unsigned)total);
    gzclose(f);

    if (readBytes != (int)total) {
        // corrupted/short file
        return false;
    }

    // notify renderer for a full refresh
    if (level->renderer) {
        levelRenderer_allChanged(level, level->renderer);
    }
    return true;
}

void Level_save(const Level* level) {
    gzFile f = gzopen("level.dat", "wb");
    if (!f) return;
    gzwrite(f, level->blocks, (unsigned)(level->width * level->height * level->depth));
    gzclose(f);
}

static void notifyNeighborChanged(Level* level, int x, int y, int z, int type) {
    if (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height) return;
    int id = Level_getTile(level, x, y, z);
    const Tile* t = (id >= 0 && id < 256) ? gTiles[id] : NULL;
    if (t && t->neighborChanged) t->neighborChanged(t, level, x, y, z, type);
}

bool level_setTile(Level* level, int x, int y, int z, int type) {
    if (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height) return false;

    int index = (y * level->height + z) * level->width + x;
    if (level->blocks[index] == (byte)type) return false;

    level->blocks[index] = (byte)type;

    notifyNeighborChanged(level, x - 1, y, z, type);
    notifyNeighborChanged(level, x + 1, y, z, type);
    notifyNeighborChanged(level, x, y - 1, z, type);
    notifyNeighborChanged(level, x, y + 1, z, type);
    notifyNeighborChanged(level, x, y, z - 1, type);
    notifyNeighborChanged(level, x, y, z + 1, type);

    calcLightDepths(level, x, z, 1, 1);
    if (level->renderer) levelRenderer_tileChanged(level->renderer, x, y, z);
    return true;
}

// No neighbor notification, no light recalc, no renderer refresh — used by
// liquid tick reactions to avoid cascading recursion (matches Java exactly).
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

AABB Level_getTilePickAABB(const Level* level, int x, int y, int z) {
    (void)level;
    return AABB_create(x, y, z, x+1, y+1, z+1);
}

bool Level_isLit(const Level* level, int x, int y, int z) {
    return (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height) ||
           (y >= level->lightDepths[x + z * level->width]);
}

void Level_onTick(Level* level) {
    level->unprocessed += level->width * level->height * level->depth;
    int ticks = level->unprocessed / 200; // c0.0.13a halved TILE_UPDATE_INTERVAL from 400
    level->unprocessed -= ticks * 200;

    for (int i = 0; i < ticks; ++i) {
        int x = rand() % level->width;
        int y = rand() % level->depth;
        int z = rand() % level->height;
        int id = Level_getTile(level, x, y, z);
        const Tile* t = (id >= 0 && id < 256) ? gTiles[id] : NULL;
        if (t && t->onTick) t->onTick(t, level, x, y, z);
    }
}