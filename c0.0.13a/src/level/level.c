// level/level.c

#include "level.h"
#include "level_renderer.h"
#include "tile/tile.h"

#include <zlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

extern void Minecraft_beginLevelLoading(const char* title);
extern void Minecraft_levelLoadUpdate(const char* status);

void Level_init(Level* level) {
    memset(level, 0, sizeof(*level));
    JRandom_initRandom(&level->random);
    level->randValue = JRandom_nextInt(&level->random);
    level->unprocessed = 0;
}

void Level_setData(Level* level, int w, int d, int h, byte* blocks) {
    free(level->blocks);
    free(level->lightDepths);
    level->width = w;
    level->height = h;
    level->depth = d;
    level->blocks = blocks;
    level->lightDepths = (int*)calloc((size_t)w * h, sizeof(int));
    if (!level->lightDepths) {
        fprintf(stderr, "Failed to allocate level memory\n");
        exit(EXIT_FAILURE);
    }
    calcLightDepths(level, 0, 0, w, h);
    if (level->renderer) LevelRenderer_allChanged(level->renderer);
}

// x1 and z1 are counts, not end coordinates
void calcLightDepths(Level* level, int x0, int z0, int x1, int z1) {
    for (int x = x0; x < x0 + x1; x++) {
        for (int z = z0; z < z0 + z1; z++) {
            int oldDepth = level->lightDepths[x + z * level->width];
            int y = level->depth - 1;
            while (y > 0 && !Level_isLightBlocker(level, x, y, z)) y--;
            level->lightDepths[x + z * level->width] = y + 1;

            // compares the old depth with y, not y + 1
            if (oldDepth != y && level->renderer) {
                int yl0 = oldDepth < y ? oldDepth : y;
                int yl1 = oldDepth > y ? oldDepth : y;
                levelRenderer_lightColumnChanged(level->renderer, x, z, yl0, yl1);
            }
        }
    }
}

bool Level_isLightBlocker(const Level* level, int x, int y, int z) {
    const Tile* t = gTiles[Level_getTile(level, x, y, z)];
    if (!t) return false;
    return t->blocksLight(t);
}

bool Level_isSolidTile(const Level* level, int x, int y, int z) {
    const Tile* t = gTiles[Level_getTile(level, x, y, z)];
    if (!t) return false;
    return t->isSolid(t);
}

static bool containsLiquidType(const Level* level, const AABB* box, int liquidId, bool any) {
    int x0 = (int)floor(box->minX);
    int x1 = (int)floor(box->maxX + 1.0f);
    int y0 = (int)floor(box->minY);
    int y1 = (int)floor(box->maxY + 1.0f);
    int z0 = (int)floor(box->minZ);
    int z1 = (int)floor(box->maxZ + 1.0f);

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (z0 < 0) z0 = 0;
    if (x1 > level->width)  x1 = level->width;
    if (y1 > level->depth)  y1 = level->depth;
    if (z1 > level->height) z1 = level->height;

    for (int x = x0; x < x1; ++x)
    for (int y = y0; y < y1; ++y)
    for (int z = z0; z < z1; ++z) {
        const Tile* t = gTiles[Level_getTile(level, x, y, z)];
        if (t && (any ? t->liquidType > 0 : t->liquidType == liquidId)) return true;
    }
    return false;
}

bool Level_containsAnyLiquid(const Level* level, const AABB* box) {
    return containsLiquidType(level, box, 0, true);
}

bool Level_containsLiquid(const Level* level, const AABB* box, int liquidId) {
    return containsLiquidType(level, box, liquidId, false);
}

void Level_destroy(Level* level) {
    free(level->blocks);
    free(level->lightDepths);
    level->blocks = NULL;
    level->lightDepths = NULL;
}

ArrayList_AABB Level_getCubes(const Level* level, const AABB* aabb) {
    ArrayList_AABB out = { .size = 0, .capacity = 16 };
    out.aabbs = (AABB*)malloc((size_t)out.capacity * sizeof(AABB));

    int x0 = (int)floor(aabb->minX);
    int x1 = (int)floor(aabb->maxX + 1.0f);
    int y0 = (int)floor(aabb->minY);
    int y1 = (int)floor(aabb->maxY + 1.0f);
    int z0 = (int)floor(aabb->minZ);
    int z1 = (int)floor(aabb->maxZ + 1.0f);

    for (int x = x0; x < x1; ++x)
    for (int y = y0; y < y1; ++y)
    for (int z = z0; z < z1; ++z) {
        AABB box;
        int haveBox = 0;

        if (x >= 0 && y >= 0 && z >= 0 && x < level->width && y < level->depth && z < level->height) {
            const Tile* t = gTiles[Level_getTile(level, x, y, z)];
            if (t && t->getAABB(t, x, y, z, &box)) haveBox = 1;
        } else if (x < 0 || y < 0 || z < 0 || x >= level->width || z >= level->height) {
            haveBox = TILE_UNBREAKABLE.getAABB(&TILE_UNBREAKABLE, x, y, z, &box);
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

// level.dat is a gzipped big endian DataOutputStream: magic, version, name, creator,
// create time, width, height, depth as shorts, then the blocks

#define LEVEL_MAGIC 656127880

static int readI16(gzFile f, int* out) {
    unsigned char b[2];
    if (gzread(f, b, 2) != 2) return 0;
    *out = (short)((b[0] << 8) | b[1]);
    return 1;
}

static int readI64(gzFile f, long long* out) {
    unsigned char b[8];
    if (gzread(f, b, 8) != 8) return 0;
    unsigned long long v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | b[i];
    *out = (long long)v;
    return 1;
}

static int readUTF(gzFile f, char* out, size_t outSize) {
    unsigned char lb[2];
    if (gzread(f, lb, 2) != 2) return 0;
    size_t len = (size_t)((lb[0] << 8) | lb[1]);

    size_t toCopy = len < outSize - 1 ? len : outSize - 1;
    if (toCopy > 0 && gzread(f, out, (unsigned)toCopy) != (int)toCopy) return 0;
    out[toCopy] = '\0';

    size_t remaining = len - toCopy;
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
    for (int i = 0; i < 8; ++i) b[i] = (unsigned char)((unsigned long long)v >> (56 - i * 8));
    gzwrite(f, b, 8);
}

static void writeUTF(gzFile f, const char* s) {
    size_t len = strlen(s);
    writeU16(f, (unsigned short)len);
    gzwrite(f, s, (unsigned)len);
}

// LevelIO.load
bool Level_load(Level* level, const char* path) {
    Minecraft_beginLevelLoading("Loading level");
    Minecraft_levelLoadUpdate("Reading..");

    gzFile f = gzopen(path, "rb");
    if (!f) return false;
    // GZIPInputStream fails on a file that isn't gzipped, zlib would read it raw
    if (gzdirect(f)) { gzclose(f); return false; }

    unsigned char header[5];
    if (gzread(f, header, 5) != 5) { gzclose(f); return false; }
    int magic = (header[0] << 24) | (header[1] << 16) | (header[2] << 8) | header[3];
    if (magic != LEVEL_MAGIC || (signed char)header[4] > 1) { gzclose(f); return false; }

    char name[64], creator[64];
    long long createTime;
    if (!readUTF(f, name, sizeof(name)) || !readUTF(f, creator, sizeof(creator)) || !readI64(f, &createTime)) {
        gzclose(f);
        return false;
    }

    int w, h, d;
    if (!readI16(f, &w) || !readI16(f, &h) || !readI16(f, &d) || w <= 0 || h <= 0 || d <= 0) { gzclose(f); return false; }

    size_t total = (size_t)w * h * d;
    byte* blocks = (byte*)malloc(total);
    if (!blocks) { gzclose(f); return false; }
    if (gzread(f, blocks, (unsigned)total) != (int)total) {
        free(blocks);
        gzclose(f);
        return false;
    }
    gzclose(f);

    Level_setData(level, w, d, h, blocks);
    memcpy(level->name, name, sizeof(name));
    memcpy(level->creator, creator, sizeof(creator));
    level->createTime = createTime;
    return true;
}

// LevelIO.loadLegacy, a raw 256x256x64 block dump
bool Level_loadLegacy(Level* level, const char* path) {
    Minecraft_beginLevelLoading("Loading level");
    Minecraft_levelLoadUpdate("Reading..");

    gzFile f = gzopen(path, "rb");
    if (!f) return false;
    // GZIPInputStream fails on a file that isn't gzipped, zlib would read it raw
    if (gzdirect(f)) { gzclose(f); return false; }

    int w = 256, h = 256, d = 64;
    size_t total = (size_t)w * h * d;
    byte* blocks = (byte*)malloc(total);
    if (!blocks) { gzclose(f); return false; }
    if (gzread(f, blocks, (unsigned)total) != (int)total) {
        free(blocks);
        gzclose(f);
        return false;
    }
    gzclose(f);

    Level_setData(level, w, d, h, blocks);
    snprintf(level->name, sizeof(level->name), "--");
    snprintf(level->creator, sizeof(level->creator), "unknown");
    level->createTime = 0;
    return true;
}

void Level_save(const Level* level, const char* path) {
    gzFile f = gzopen(path, "wb");
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

static void neighborChanged(Level* level, int x, int y, int z, int type) {
    if (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height) return;
    const Tile* t = gTiles[level->blocks[(y * level->height + z) * level->width + x]];
    if (t) t->neighborChanged(t, level, x, y, z, type);
}

bool level_setTile(Level* level, int x, int y, int z, int type) {
    if (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height) return false;

    int index = (y * level->height + z) * level->width + x;
    if (type == level->blocks[index]) return false;
    level->blocks[index] = (byte)type;

    neighborChanged(level, x - 1, y, z, type);
    neighborChanged(level, x + 1, y, z, type);
    neighborChanged(level, x, y - 1, z, type);
    neighborChanged(level, x, y + 1, z, type);
    neighborChanged(level, x, y, z - 1, type);
    neighborChanged(level, x, y, z + 1, type);

    calcLightDepths(level, x, z, 1, 1);
    if (level->renderer) levelRenderer_tileChanged(level->renderer, x, y, z);
    return true;
}

bool Level_setTileNoUpdate(Level* level, int x, int y, int z, int type) {
    if (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height) return false;
    int index = (y * level->height + z) * level->width + x;
    if (type == level->blocks[index]) return false;
    level->blocks[index] = (byte)type;
    return true;
}

int Level_getTile(const Level* level, int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height)
        return 0;
    return level->blocks[(y * level->height + z) * level->width + x];
}

bool Level_isLit(const Level* level, int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height) return true;
    return y >= level->lightDepths[x + z * level->width];
}

void Level_onTick(Level* level) {
    level->unprocessed += level->width * level->height * level->depth;
    int ticks = level->unprocessed / 200;
    level->unprocessed -= ticks * 200;

    // the tick lcg, overflowing like a 32 bit int
    for (int i = 0; i < ticks; ++i) {
        level->randValue = (int)((unsigned)level->randValue * 1664525u + 1013904223u);
        int x = level->randValue >> 16 & (level->width - 1);
        level->randValue = (int)((unsigned)level->randValue * 1664525u + 1013904223u);
        int y = level->randValue >> 16 & (level->depth - 1);
        level->randValue = (int)((unsigned)level->randValue * 1664525u + 1013904223u);
        int z = level->randValue >> 16 & (level->height - 1);
        int id = level->blocks[(y * level->height + z) * level->width + x];
        if (gTiles[id] && gTiles[id]->onTick) gTiles[id]->onTick(gTiles[id], level, x, y, z);
    }
}
