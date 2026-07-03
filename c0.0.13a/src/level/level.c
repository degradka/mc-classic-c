// level.c: world storage, lighting columns, IO, and solid cube queries

#include "level.h"
#include "level_renderer.h"
#include "tile/tile.h"
#include "levelgen/level_gen.h"

#include <zlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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

    // Level_load frees and reallocates blocks/lightDepths at the file's own
    // dimensions if they differ from the boot size passed in above
    bool mapLoaded = Level_load(level);
    if (!mapLoaded) {
        Level_generateMap(level);
    }

    calcLightDepths(level, 0, 0, level->width, level->height);
}

// used by the pause menu's Generate new level, which regenerates at a
// different size than the boot map, matching Minecraft.generateNewLevel()
void Level_resize(Level* level, int width, int height, int depth) {
    LevelRenderer* renderer = level->renderer;
    level->renderer = NULL; // detach so calcLightDepths below does not touch the stale renderer

    free(level->blocks);
    free(level->lightDepths);

    level->width = width;
    level->height = height;
    level->depth = depth;
    level->unprocessed = 0;

    level->blocks = (byte*)malloc((size_t)width * height * depth);
    level->lightDepths = (int*)malloc((size_t)width * height * sizeof(int));
    if (!level->blocks || !level->lightDepths) {
        fprintf(stderr, "Failed to allocate level memory\n");
        exit(EXIT_FAILURE);
    }

    Level_generateMap(level);
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
    // c0.0.13a replaced the old Perlin hills generator with flat terrain,
    // carved caves and flood filled lakes (see level_gen.h for why it's flat).
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

    int x0 = (int)floor(aabb->minX);
    int x1 = (int)floor(aabb->maxX + 1.0);
    int y0 = (int)floor(aabb->minY);
    int y1 = (int)floor(aabb->maxY + 1.0);
    int z0 = (int)floor(aabb->minZ);
    int z1 = (int)floor(aabb->maxZ + 1.0);

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
            // outside the map horizontally: an invisible unbreakable wall.
            // vertically out of bounds (below the floor or above the map) is
            // deliberately left open, matching Java exactly
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

// level.dat is a gzipped stream of Java DataOutputStream primitives, all
// big endian: magic, version, three UTF strings/longs of save metadata,
// width/height/depth as shorts, then the raw block bytes

#define LEVEL_MAGIC 656127880

static int readU16(gzFile f, unsigned short* out) {
    unsigned char b[2];
    if (gzread(f, b, 2) != 2) return 0;
    *out = (unsigned short)((b[0] << 8) | b[1]);
    return 1;
}

static int readI64(gzFile f, long long* out) {
    unsigned char b[8];
    if (gzread(f, b, 8) != 8) return 0;
    long long v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | b[i];
    *out = v;
    return 1;
}

static int readUTF(gzFile f, char* out, size_t outSize) {
    unsigned short len;
    if (!readU16(f, &len)) return 0;

    size_t toCopy = ((size_t)len < outSize - 1) ? (size_t)len : outSize - 1;
    if (toCopy > 0 && gzread(f, out, (unsigned)toCopy) != (int)toCopy) return 0;
    out[toCopy] = '\0';

    size_t remaining = (size_t)len - toCopy;
    char discard[64];
    while (remaining > 0) {
        size_t chunk = remaining < sizeof(discard) ? remaining : sizeof(discard);
        if (gzread(f, discard, (unsigned)chunk) != (int)chunk) return 0;
        remaining -= chunk;
    }
    return 1;
}

static void writeU16(gzFile f, unsigned short v) {
    unsigned char b[2] = { (unsigned char)(v >> 8), (unsigned char)v };
    gzwrite(f, b, 2);
}

static void writeI64(gzFile f, long long v) {
    unsigned char b[8];
    for (int i = 0; i < 8; ++i) b[i] = (unsigned char)(v >> (56 - i * 8));
    gzwrite(f, b, 8);
}

static void writeUTF(gzFile f, const char* s) {
    size_t len = strlen(s);
    writeU16(f, (unsigned short)len);
    gzwrite(f, s, (unsigned)len);
}

bool Level_load(Level* level) {
    gzFile f = gzopen("level.dat", "rb");
    if (!f) return false;

    unsigned char header[5];
    if (gzread(f, header, 5) != 5) { gzclose(f); return false; }
    int magic = (header[0] << 24) | (header[1] << 16) | (header[2] << 8) | header[3];
    if (magic != LEVEL_MAGIC || header[4] > 1) { gzclose(f); return false; }

    char name[64], creator[64];
    long long createTime;
    if (!readUTF(f, name, sizeof(name)) ||
        !readUTF(f, creator, sizeof(creator)) ||
        !readI64(f, &createTime)) {
        gzclose(f);
        return false;
    }

    unsigned short w, h, d;
    if (!readU16(f, &w) || !readU16(f, &h) || !readU16(f, &d)) { gzclose(f); return false; }

    size_t total = (size_t)w * h * d;
    byte* blocks = (byte*)malloc(total);
    if (!blocks) { gzclose(f); return false; }

    if (gzread(f, blocks, (unsigned)total) != (int)total) {
        free(blocks);
        gzclose(f);
        return false;
    }
    gzclose(f);

    free(level->blocks);
    free(level->lightDepths);

    level->width  = w;
    level->height = h;
    level->depth  = d;
    level->blocks = blocks;
    memcpy(level->name, name, sizeof(name));
    memcpy(level->creator, creator, sizeof(creator));
    level->createTime = createTime;

    level->lightDepths = (int*)malloc((size_t)w * h * sizeof(int));
    if (!level->lightDepths) {
        fprintf(stderr, "Failed to allocate level memory\n");
        exit(EXIT_FAILURE);
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

    unsigned char header[5] = {
        (unsigned char)(LEVEL_MAGIC >> 24), (unsigned char)(LEVEL_MAGIC >> 16),
        (unsigned char)(LEVEL_MAGIC >> 8),  (unsigned char)LEVEL_MAGIC,
        1
    };
    gzwrite(f, header, 5);

    writeUTF(f, level->name);
    writeUTF(f, level->creator);
    writeI64(f, level->createTime);

    writeU16(f, (unsigned short)level->width);
    writeU16(f, (unsigned short)level->height);
    writeU16(f, (unsigned short)level->depth);

    gzwrite(f, level->blocks, (unsigned)((size_t)level->width * level->height * level->depth));
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

// No neighbor notification, no light recalc, no renderer refresh. Used by
// liquid tick reactions to avoid cascading recursion, matching Java exactly.
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