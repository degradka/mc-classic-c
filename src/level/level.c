// level/level.c — world storage, lighting columns, IO, and solid-cube queries

#include "level.h"
#include "level_renderer.h"
#include "tile/tile.h"
#include "perlin.h"

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
    level->randValue = (unsigned)rand();

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
            level->lightDepths[x + z * level->width] = d + 1;

            if (prev != d && level->renderer) {
                int ylMin = prev < d ? prev : d;
                int ylMax = prev > d ? prev : d;
                LevelRenderer_lightColumnChanged(level->renderer, x, z, ylMin, ylMax);
            }
        }
    }
}

void Level_generateMap(Level* level) {
    int w = level->width, h = level->height, d = level->depth;

    int* first  = (int*)malloc((size_t)w * h * sizeof(int));
    int* second = (int*)malloc((size_t)w * h * sizeof(int));
    int* cliff  = (int*)malloc((size_t)w * h * sizeof(int));
    int* rock   = (int*)malloc((size_t)w * h * sizeof(int));
    if (!first || !second || !cliff || !rock) {
        fprintf(stderr, "Perlin temp alloc failed\n");
        if (first)  free(first);
        if (second) free(second);
        if (cliff)  free(cliff);
        if (rock)   free(rock);
        return;
    }

    Perlin_read(w, h, 0, first);
    Perlin_read(w, h, 0, second);
    Perlin_read(w, h, 1, cliff);
    Perlin_read(w, h, 1, rock);

    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < d; ++y) {
            for (int z = 0; z < h; ++z) {
                int idx2D = x + z * w;
                int f = first [idx2D];
                int s = second[idx2D];

                // If cliff noise < 128, force to first height
                if (cliff[idx2D] < 128) s = f;

                int maxLevelHeight = ( (s > f ? s : f) ) / 8 + d / 3;
                int maxRockHeight  =  rock[idx2D] / 8 + d / 3;
                if (maxRockHeight > maxLevelHeight - 2) {
                    maxRockHeight = maxLevelHeight - 2;
                }

                int index = (y * h + z) * w + x;
                int id = 0;

                if (y == maxLevelHeight) id = 2; // grass
                if (y <  maxLevelHeight) id = 3; // dirt
                if (y <= maxRockHeight)  id = 1; // rock

                level->blocks[index] = (unsigned char)id;
            }
        }
    }

    free(first); free(second); free(cliff); free(rock);
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

void Level_destroy(Level* level) {
    free(level->blocks);
    free(level->lightDepths);
}

ArrayList_AABB Level_getCubes(const Level* level, const AABB* aabb) {
    ArrayList_AABB out = { .size = 0, .capacity = 16 };
    out.aabbs = (AABB*)malloc((size_t)out.capacity * sizeof(AABB));

    int x0 = (int)floor(aabb->minX);
    int x1 = (int)floor(aabb->maxX + 1.0);
    int y0 = (int)floor(aabb->minY);
    int y1 = (int)floor(aabb->maxY + 1.0);
    int z0 = (int)floor(aabb->minZ);
    int z1 = (int)floor(aabb->maxZ + 1.0);

    for (int x = x0; x < x1; ++x)
    for (int y = y0; y < y1; ++y)
    for (int z = z0; z < z1; ++z) {

        const int in =
            (x >= 0 && y >= 0 && z >= 0 &&
             x < level->width && y < level->depth && z < level->height);

        if (in) {
            int id = Level_getTile(level, x, y, z);
            const Tile* t = (id >= 0 && id < 256) ? gTiles[id] : NULL;
            if (!t) continue;

            // ask the tile for its collision box
            AABB box;
            if (t->getAABB && t->getAABB(t, x, y, z, &box)) {
                if (out.size == out.capacity) {
                    out.capacity *= 2;
                    out.aabbs = (AABB*)realloc(out.aabbs, (size_t)out.capacity * sizeof(AABB));
                }
                out.aabbs[out.size++] = box;
            }
        } else {
            // out-of-bounds: use unbreakable tile’s AABB (id 7)
            const Tile* ub = gTiles[7];
            if (ub && ub->getAABB) {
                AABB box;
                if (ub->getAABB(ub, x, y, z, &box)) {
                    if (out.size == out.capacity) {
                        out.capacity *= 2;
                        out.aabbs = (AABB*)realloc(out.aabbs, (size_t)out.capacity * sizeof(AABB));
                    }
                    out.aabbs[out.size++] = box;
                }
            }
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
        LevelRenderer_allChanged(level, level->renderer);
    }
    return true;
}

void Level_save(const Level* level) {
    gzFile f = gzopen("level.dat", "wb");
    if (!f) return;
    gzwrite(f, level->blocks, (unsigned)(level->width * level->height * level->depth));
    gzclose(f);
}

bool Level_setTile(Level* level, int x, int y, int z, int type) {
    if (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height) return false;

    int index = (y * level->height + z) * level->width + x;
    if (level->blocks[index] == (byte)type) return false;

    level->blocks[index] = (byte)type;

    Level_neighborChanged(level, x-1, y,   z,   type);
    Level_neighborChanged(level, x+1, y,   z,   type);
    Level_neighborChanged(level, x,   y-1, z,   type);
    Level_neighborChanged(level, x,   y+1, z,   type);
    Level_neighborChanged(level, x,   y,   z-1, type);
    Level_neighborChanged(level, x,   y,   z+1, type);

    calcLightDepths(level, x, z, 1, 1);
    if (level->renderer) LevelRenderer_tileChanged(level->renderer, x, y, z);
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
    int ticks = level->unprocessed / 200;   // was 400
    level->unprocessed -= ticks * 200;

    for (int i = 0; i < ticks; ++i) {
        // LCG like Java
        level->randValue = level->randValue * 1664525u + 1013904223u;
        int x = (level->randValue >> 16) & (level->width  - 1);
        level->randValue = level->randValue * 1664525u + 1013904223u;
        int y = (level->randValue >> 16) & (level->depth  - 1);
        level->randValue = level->randValue * 1664525u + 1013904223u;
        int z = (level->randValue >> 16) & (level->height - 1);

        int id = Level_getTile(level, x, y, z);
        const Tile* t = (id >= 0 && id < 256) ? gTiles[id] : NULL;
        if (t && t->onTick) t->onTick(t, level, x, y, z);
    }
}

bool Level_setTileNoUpdate(Level* level, int x, int y, int z, int type) {
    if (x < 0 || y < 0 || z < 0 ||
        x >= level->width || y >= level->depth || z >= level->height) return false;

    int index = (y * level->height + z) * level->width + x;
    if (level->blocks[index] == (byte)type) return false;

    level->blocks[index] = (byte)type;
    return true;
}

void Level_neighborChanged(Level* level, int x, int y, int z, int changedType) {
    if (x < 0 || y < 0 || z < 0 ||
        x >= level->width || y >= level->depth || z >= level->height) return;

    int id = Level_getTile(level, x, y, z);
    const Tile* t = (id >= 0 && id < 256) ? gTiles[id] : NULL;
    if (t && t->neighborChanged) {
        t->neighborChanged(t, level, x, y, z, changedType);
    }
}

bool Level_containsAnyLiquid(const Level* level, const AABB* box) {
    int x0 = (int)floorf(box->minX);
    int x1 = (int)floorf(box->maxX + 1.f);
    int y0 = (int)floorf(box->minY);
    int y1 = (int)floorf(box->maxY + 1.f);
    int z0 = (int)floorf(box->minZ);
    int z1 = (int)floorf(box->maxZ + 1.f);

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
        if (t && t->getLiquidType && t->getLiquidType(t) > 0) return true;
    }
    return false;
}

bool Level_containsLiquid(const Level* level, const AABB* box, int liquidId) {
    int x0 = (int)floorf(box->minX);
    int x1 = (int)floorf(box->maxX + 1.f);
    int y0 = (int)floorf(box->minY);
    int y1 = (int)floorf(box->maxY + 1.f);
    int z0 = (int)floorf(box->minZ);
    int z1 = (int)floorf(box->maxZ + 1.f);

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
        if (t && t->getLiquidType && t->getLiquidType(t) == liquidId) return true;
    }
    return false;
}