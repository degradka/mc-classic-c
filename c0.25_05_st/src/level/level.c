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
#include <float.h>

void Level_init(Level* level, int width, int height, int depth) {
    level->width = width;
    level->height = height;
    level->depth = depth;
    level->renderer = NULL;
    level->unprocessed = 0;
    level->xSpawn = level->ySpawn = level->zSpawn = 0;
    level->rotSpawn = 0.0f;
    level->tickRandom = (unsigned int)rand();
    level->tickCount = 0;
    level->tickList = NULL;
    level->tickListSize = 0;
    level->tickListCapacity = 0;
    level->networkMode = false;
    level->player = NULL; // set once by Minecraft's own init, via Level_setPlayer

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
    if (level->xSpawn == 0 && level->ySpawn == 0 && level->zSpawn == 0) Level_findSpawn(level);
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
    level->xSpawn = level->ySpawn = level->zSpawn = 0;
    level->rotSpawn = 0.0f;
    free(level->tickList);
    level->tickList = NULL;
    level->tickListSize = level->tickListCapacity = 0;
    level->networkMode = false; // regenerating always returns to local/singleplayer authority

    level->blocks = (byte*)malloc((size_t)width * height * depth);
    level->lightDepths = (int*)malloc((size_t)width * height * sizeof(int));
    if (!level->blocks || !level->lightDepths) {
        fprintf(stderr, "Failed to allocate level memory\n");
        exit(EXIT_FAILURE);
    }

    Level_generateMap(level);
    calcLightDepths(level, 0, 0, width, height);
    Level_findSpawn(level);

    level->renderer = renderer;
}

// installs a level received over the network: raw decompressed block bytes,
// no save metadata and no generation. Spawn position isn't part of this
// transfer either, it arrives after as a separate SpawnPlayer packet, so
// the caller must not rely on xSpawn/ySpawn/zSpawn until that lands
void Level_setDataFromNetwork(Level* level, int width, int height, int depth, const byte* blocks) {
    LevelRenderer* renderer = level->renderer;
    level->renderer = NULL;

    free(level->blocks);
    free(level->lightDepths);

    level->width = width;
    level->height = height;
    level->depth = depth;
    level->unprocessed = 0;
    level->xSpawn = level->ySpawn = level->zSpawn = 0;
    level->rotSpawn = 0.0f;
    free(level->tickList);
    level->tickList = NULL;
    level->tickListSize = level->tickListCapacity = 0;
    // c0.0.19a_04: a network-installed level is server-authoritative for its
    // whole lifetime, matching new Level().setNetworkMode(true) in the real
    // source's LevelFinalize handler
    level->networkMode = true;

    size_t total = (size_t)width * height * depth;
    level->blocks = (byte*)malloc(total);
    level->lightDepths = (int*)malloc((size_t)width * height * sizeof(int));
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
    free(level->tickList);
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

void Level_updateNeighborsAt(Level* level, int x, int y, int z) {
    notifyNeighborChanged(level, x, y, z, Level_getTile(level, x, y, z));
}

bool level_setTile(Level* level, int x, int y, int z, int type) {
    // c0.0.19a_04: gated no-op in multiplayer, matching the real source's
    // setTile/setTileNoNeighborChange checking networkMode. Only the
    // server's own echoed SetBlock (via Level_netSetTile) may actually
    // change tiles once a level has been installed from a network connection
    if (level->networkMode) return false;
    return Level_netSetTile(level, x, y, z, type);
}

bool Level_netSetTile(Level* level, int x, int y, int z, int type) {
    if (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height) return false;

    int index = (y * level->height + z) * level->width + x;
    int oldType = level->blocks[index];
    if (oldType == (byte)type) return false;

    level->blocks[index] = (byte)type;

    // c0.0.19a_04: onPlace/onRemoved hooks, added for Sponge
    const Tile* oldTile = (oldType >= 0 && oldType < 256) ? gTiles[oldType] : NULL;
    if (oldTile && oldTile->onRemoved) oldTile->onRemoved(oldTile, level, x, y, z);
    const Tile* newTile = (type >= 0 && type < 256) ? gTiles[type] : NULL;
    if (newTile && newTile->onPlace) newTile->onPlace(newTile, level, x, y, z);

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

// mob AI line of sight check, matches Level.clip(Vec3,Vec3) exactly: the
// real source's tile collision type getter defaults to SOLID for every
// tile and is only overridden by the liquid tile class, so the collision
// type equals SOLID test it does is really just testing for not a liquid,
// the same as this port's own mayPick, shared with the block picking
// raycast in minecraft.c, both use the exact same real method. Amanatides
// Woo voxel walk, same technique as that other raycast, but capped to the
// actual distance between the two points instead of a fixed reach
bool Level_clip(const Level* level, float x1, float y1, float z1, float x2, float y2, float z2) {
    double dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
    double dist = sqrt(dx*dx + dy*dy + dz*dz);
    if (dist < 1e-5) return false;
    dx /= dist; dy /= dist; dz /= dist;

    int x = (int)floor(x1), y = (int)floor(y1), z = (int)floor(z1);
    const int stepX = (dx > 0) - (dx < 0);
    const int stepY = (dy > 0) - (dy < 0);
    const int stepZ = (dz > 0) - (dz < 0);

    double tMaxX = (dx != 0.0) ? (((stepX > 0 ? (x + 1) : x) - x1) / dx) : DBL_MAX;
    double tMaxY = (dy != 0.0) ? (((stepY > 0 ? (y + 1) : y) - y1) / dy) : DBL_MAX;
    double tMaxZ = (dz != 0.0) ? (((stepZ > 0 ? (z + 1) : z) - z1) / dz) : DBL_MAX;

    double tDeltaX = (dx != 0.0) ? fabs(1.0 / dx) : DBL_MAX;
    double tDeltaY = (dy != 0.0) ? fabs(1.0 / dy) : DBL_MAX;
    double tDeltaZ = (dz != 0.0) ? fabs(1.0 / dz) : DBL_MAX;

    double t = 0.0;
    while (t <= dist) {
        int id = Level_getTile(level, x, y, z);
        if (id > 0 && gTiles[id] && gTiles[id]->mayPick(gTiles[id])) return true;

        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) { x += stepX; t = tMaxX; tMaxX += tDeltaX; }
            else               { z += stepZ; t = tMaxZ; tMaxZ += tDeltaZ; }
        } else {
            if (tMaxY < tMaxZ) { y += stepY; t = tMaxY; tMaxY += tDeltaY; }
            else               { z += stepZ; t = tMaxZ; tMaxZ += tDeltaZ; }
        }
    }
    return false;
}

bool Level_maybeGrowTree(Level* level, int x, int y, int z) {
    int trunkHeight = rand() % 3 + 4;

    // space check: exact column at the base, 3x3 through the trunk, 5x5 for
    // the canopy's own top 2 layers, matching the real source's own radius
    // schedule exactly. checkRadius is 0 at the base tile itself, 2 for the
    // top 2 layers, 1 everywhere in between
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

    // canopy: bottom 2 layers are a 5x5 (radius 2), top 2 layers a 3x3
    // (radius 1); the outermost diagonal corner of each layer is dropped
    // half the time, except on the very top layer where it's always dropped
    for (int ly = y - 3 + trunkHeight; ly <= y + trunkHeight; ly++) {
        int fromTop = ly - (y + trunkHeight); // -3..0, 0 = the very top layer
        int radius = 1 - fromTop / 2;
        for (int lx = x - radius; lx <= x + radius; lx++) {
            int dx = lx - x;
            for (int lz = z - radius; lz <= z + radius; lz++) {
                int dz = lz - z;
                if (abs(dx) == radius && abs(dz) == radius && (rand() % 2 == 0 || fromTop == 0)) continue;
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
    // real value changed from 0.5f in c0.0.14a_08 to 0.6f here, confirmed
    // against both versions' actual decompiled Level.getBrightness
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
    return (float)(level->depth / 2 - 2);
}

float Level_getWaterLevel(const Level* level) {
    return (float)(level->depth / 2);
}

// picks a random column near the map center and spawns on its topmost solid
// tile once it's above water level. The real source has a byte overflow
// retry cap here that can never actually trigger (compared against the
// literal 10000, but the counter is a byte that wraps at 127), so it's not
// ported. Terrain above water is found almost immediately on any normal map.
void Level_findSpawn(Level* level) {
    while (1) {
        int x = rand() % (level->width / 2) + level->width / 4;
        int z = rand() % (level->height / 2) + level->height / 4;
        int y = Level_getHighestTile(level, x, z) + 1;
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
    // c0.0.19a_04: gated no-op in multiplayer, matching the real source.
    // Falling blocks (Sand/Gravel) are server-simulated once connected; the
    // client only sees them move via the server's own SetBlock echoes
    if (level->networkMode) return;
    int a = Level_getTile(level, x1, y1, z1);
    int b = Level_getTile(level, x2, y2, z2);
    Level_setTileNoUpdate(level, x1, y1, z1, b);
    Level_setTileNoUpdate(level, x2, y2, z2, a);

    notifyNeighborChanged(level, x1 - 1, y1, z1, b);
    notifyNeighborChanged(level, x1 + 1, y1, z1, b);
    notifyNeighborChanged(level, x1, y1 - 1, z1, b);
    notifyNeighborChanged(level, x1, y1 + 1, z1, b);
    notifyNeighborChanged(level, x1, y1, z1 - 1, b);
    notifyNeighborChanged(level, x1, y1, z1 + 1, b);

    notifyNeighborChanged(level, x2 - 1, y2, z2, a);
    notifyNeighborChanged(level, x2 + 1, y2, z2, a);
    notifyNeighborChanged(level, x2, y2 - 1, z2, a);
    notifyNeighborChanged(level, x2, y2 + 1, z2, a);
    notifyNeighborChanged(level, x2, y2, z2 - 1, a);
    notifyNeighborChanged(level, x2, y2, z2 + 1, a);

    calcLightDepths(level, x1, z1, 1, 1);
    calcLightDepths(level, x2, z2, 1, 1);
    if (level->renderer) {
        levelRenderer_tileChanged(level->renderer, x1, y1, z1);
        levelRenderer_tileChanged(level->renderer, x2, y2, z2);
    }
}

static void pushTickEntry(Level* level, TickEntry entry) {
    if (level->tickListSize == level->tickListCapacity) {
        level->tickListCapacity = level->tickListCapacity ? level->tickListCapacity * 2 : 16;
        level->tickList = (TickEntry*)realloc(level->tickList, (size_t)level->tickListCapacity * sizeof(TickEntry));
    }
    level->tickList[level->tickListSize++] = entry;
}

void Level_addToTickNextTick(Level* level, int x, int y, int z, int tileId) {
    TickEntry e = { x, y, z, tileId, 0 };
    if (tileId > 0 && tileId < 256 && gTiles[tileId]) e.delay = gTiles[tileId]->tickDelay;
    pushTickEntry(level, e);
}

void Level_onTick(Level* level) {
    level->tickCount++;

    if (level->tickCount % 5 == 0 && level->tickListSize > 0) {
        int n = level->tickListSize;
        // drain the queue snapshot from the front, matching the Java
        // ArrayList.remove(0) fifo drain of everything queued so far. An
        // entry still delayed (lava) gets decremented and pushed back to
        // the tail instead of firing.
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
    int ticks = level->unprocessed / 200; // c0.0.13a halved TILE_UPDATE_INTERVAL from 400
    level->unprocessed -= ticks * 200;

    // packed single-draw LCG replacing three independent rand()%dim calls,
    // bit widths sized to the level dimensions (power of 2 masks)
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
        if (t && t->onTick) t->onTick(t, level, x, y, z);
    }
}