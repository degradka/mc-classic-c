// level/level.c

#include "level.h"
#include "level_renderer.h"
#include "tile/tile.h"
#include "../character/zombie.h"
#include "../net/network_player.h"
#include <string.h>

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
    level->listCapacity = 10;
}

// ArrayList.ensureCapacity, grows by half plus one
static void growList(Level* level) {
    int need = level->entityCount + level->playerCount + 1;
    if (need <= level->listCapacity) return;
    int cap = level->listCapacity * 3 / 2 + 1;
    level->listCapacity = cap < need ? need : cap;
}

void Level_setData(Level* level, int w, int d, int h, byte* blocks) {
    free(level->blocks);
    free(level->lightDepths);
    level->width = w;
    level->height = h;
    level->depth = d;
    level->blocks = blocks;
    level->lightDepths = (int*)malloc((size_t)w * h * sizeof(int));
    if (!level->lightDepths) {
        fprintf(stderr, "Failed to allocate level memory\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < w * h; i++) level->lightDepths[i] = d;
    calcLightDepths(level, 0, 0, w, h);
    if (level->renderer) LevelRenderer_allChanged(level->renderer);
    level->tickListSize = 0;
    Level_findSpawn(level);
}

// a fresh Random each time, not the level's
void Level_findSpawn(Level* level) {
    JRandom random;
    JRandom_initRandom(&random);
    int tries = 0;
    int x, y, z;
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

void Level_setSpawnPos(Level* level, int x, int y, int z, float rot) {
    level->xSpawn = x;
    level->ySpawn = y;
    level->zSpawn = z;
    level->rotSpawn = rot;
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
    int x0 = (int)box->minX;
    int x1 = (int)box->maxX + 1;
    int y0 = (int)box->minY;
    int y1 = (int)box->maxY + 1;
    int z0 = (int)box->minZ;
    int z1 = (int)box->maxZ + 1;
    if (box->minX < 0.0f) x0--;
    if (box->minY < 0.0f) y0--;
    if (box->minZ < 0.0f) z0--;

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
    free(level->tickList);
    free(level->entities);
    free(level->players);
    level->players = NULL;
    level->playerCount = level->playerCapacity = 0;
    level->blocks = NULL;
    level->lightDepths = NULL;
    level->tickList = NULL;
    level->entities = NULL;
    level->tickListSize = level->tickListCapacity = 0;
    level->entityCount = level->entityCapacity = 0;
}

ArrayList_AABB Level_getCubes(const Level* level, const AABB* aabb) {
    ArrayList_AABB out = { .size = 0, .capacity = 16 };
    out.aabbs = (AABB*)malloc((size_t)out.capacity * sizeof(AABB));

    int x0 = (int)aabb->minX;
    int x1 = (int)aabb->maxX + 1;
    int y0 = (int)aabb->minY;
    int y1 = (int)aabb->maxY + 1;
    int z0 = (int)aabb->minZ;
    int z1 = (int)aabb->maxZ + 1;
    if (aabb->minX < 0.0f) x0--;
    if (aabb->minY < 0.0f) y0--;
    if (aabb->minZ < 0.0f) z0--;

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

void Level_saveNull(const char* path) {
    gzFile f = gzopen(path, "wb");
    if (!f) return;
    // the magic, version 2, then an object stream holding only TC_NULL
    unsigned char data[10] = {
        (unsigned char)(LEVEL_MAGIC >> 24), (unsigned char)(LEVEL_MAGIC >> 16),
        (unsigned char)(LEVEL_MAGIC >> 8),  (unsigned char)LEVEL_MAGIC,
        2, 0xAC, 0xED, 0x00, 0x05, 0x70
    };
    gzwrite(f, data, 10);
    gzclose(f);
}

static void neighborChanged(Level* level, int x, int y, int z, int type) {
    if (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height) return;
    const Tile* t = gTiles[level->blocks[(y * level->height + z) * level->width + x]];
    if (t) t->neighborChanged(t, level, x, y, z, type);
}

void Level_updateNeighborsAt(Level* level, int x, int y, int z, int type) {
    neighborChanged(level, x - 1, y, z, type);
    neighborChanged(level, x + 1, y, z, type);
    neighborChanged(level, x, y - 1, z, type);
    neighborChanged(level, x, y + 1, z, type);
    neighborChanged(level, x, y, z - 1, type);
    neighborChanged(level, x, y, z + 1, type);
}

// air at the map edge between the ground and the water level turns into water
bool Level_setTileNoNeighborChange(Level* level, int x, int y, int z, int type) {
    if (level->networkMode) return false;
    return Level_netSetTileNoNeighborChange(level, x, y, z, type);
}

// the old tile hears it's gone, then the new one that it's there
bool Level_netSetTileNoNeighborChange(Level* level, int x, int y, int z, int type) {
    if (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height) return false;
    int index = (y * level->height + z) * level->width + x;
    if (type == level->blocks[index]) return false;
    if (type == 0 && (x == 0 || z == 0 || x == level->width - 1 || z == level->height - 1) &&
        (float)y >= Level_getGroundLevel(level) && (float)y < Level_getWaterLevel(level)) {
        type = TILE_WATER.id;
    }
    int old = level->blocks[index];
    level->blocks[index] = (byte)type;
    if (old != 0) gTiles[old]->onRemoved(gTiles[old], level, x, y, z);
    if (type != 0) gTiles[type]->onAdded(gTiles[type], level, x, y, z);
    calcLightDepths(level, x, z, 1, 1);
    if (level->renderer) levelRenderer_tileChanged(level->renderer, x, y, z);
    return true;
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

bool Level_isWater(const Level* level, int x, int y, int z) {
    int id = Level_getTile(level, x, y, z);
    return id > 0 && gTiles[id]->liquidType == LIQUID_WATER;
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

float Level_getBrightness(const Level* level, int x, int y, int z) {
    return Level_isLit(level, x, y, z) ? 1.0f : 0.6f;
}

float Level_getGroundLevel(const Level* level) {
    return (float)(level->depth / 2 - 2);
}

float Level_getWaterLevel(const Level* level) {
    return (float)(level->depth / 2);
}

int Level_getHighestTile(const Level* level, int x, int z) {
    int y = level->depth;
    while ((Level_getTile(level, x, y - 1, z) == 0 || gTiles[Level_getTile(level, x, y - 1, z)]->liquidType != 0) && y > 0) y--;
    return y;
}

static bool solidAt(const Level* level, float x, float y, float z) {
    int id = Level_getTile(level, (int)x, (int)y, (int)z);
    return id > 0 && gTiles[id]->isSolid(gTiles[id]);
}

bool Level_isSolid(const Level* level, float x, float y, float z, float r) {
    if (solidAt(level, x - r, y - r, z - r)) return true;
    if (solidAt(level, x - r, y - r, z + r)) return true;
    if (solidAt(level, x - r, y + r, z - r)) return true;
    if (solidAt(level, x - r, y + r, z + r)) return true;
    if (solidAt(level, x + r, y - r, z - r)) return true;
    if (solidAt(level, x + r, y - r, z + r)) return true;
    if (solidAt(level, x + r, y + r, z - r)) return true;
    return solidAt(level, x + r, y + r, z + r);
}

bool Level_isFree(const Level* level, const AABB* box) {
    for (int i = 0; i < level->entityCount; ++i) {
        if (AABB_intersects(&level->entities[i].base.boundingBox, box)) return false;
    }
    for (int i = 0; i < level->playerCount; ++i) {
        if (AABB_intersects(&level->players[i]->base.boundingBox, box)) return false;
    }
    return true;
}

void Level_addToTickNextTick(Level* level, int x, int y, int z, int id) {
    if (level->networkMode) return;
    if (level->tickListSize == level->tickListCapacity) {
        level->tickListCapacity = level->tickListCapacity ? level->tickListCapacity * 2 : 256;
        level->tickList = (TickEntry*)realloc(level->tickList, (size_t)level->tickListCapacity * sizeof(TickEntry));
    }
    TickEntry e = { x, y, z, id, 0 };
    if (id > 0) e.delay = gTiles[id]->tickDelay;
    level->tickList[level->tickListSize++] = e;
}

void Level_addEntity(Level* level, const Zombie* z) {
    growList(level);
    if (level->entityCount == level->entityCapacity) {
        level->entityCapacity = level->entityCapacity ? level->entityCapacity * 2 : 16;
        level->entities = (Zombie*)realloc(level->entities, (size_t)level->entityCapacity * sizeof(Zombie));
    }
    level->entities[level->entityCount++] = *z;
}

void Level_addPlayer(Level* level, NetworkPlayer* p) {
    growList(level);
    if (level->playerCount == level->playerCapacity) {
        level->playerCapacity = level->playerCapacity ? level->playerCapacity * 2 : 16;
        level->players = (NetworkPlayer**)realloc(level->players, (size_t)level->playerCapacity * sizeof(NetworkPlayer*));
    }
    level->players[level->playerCount++] = p;
}

void Level_removePlayer(Level* level, NetworkPlayer* p) {
    for (int i = 0; i < level->playerCount; ++i) {
        if (level->players[i] != p) continue;
        memmove(&level->players[i], &level->players[i + 1], (size_t)(level->playerCount - i - 1) * sizeof(NetworkPlayer*));
        level->playerCount--;
        return;
    }
}

void Level_tickEntities(Level* level) {
    for (int i = 0; i < level->entityCount; ++i) {
        Zombie_onTick(&level->entities[i]);
        if (level->entities[i].base.removed) {
            memmove(&level->entities[i], &level->entities[i + 1], (size_t)(level->entityCount - i - 1) * sizeof(Zombie));
            level->entityCount--;
            i--;
        }
    }
    for (int i = 0; i < level->playerCount; ++i) {
        NetworkPlayer_onTick(level->players[i]);
        if (level->players[i]->base.removed) {
            memmove(&level->players[i], &level->players[i + 1], (size_t)(level->playerCount - i - 1) * sizeof(NetworkPlayer*));
            level->playerCount--;
            i--;
        }
    }
}

void Level_onTick(Level* level) {
    level->tickCount++;

    int wBits = 1, hBits = 1;
    while (1 << wBits < level->width) wBits++;
    while (1 << hBits < level->height) hBits++;
    int hMask = level->height - 1;
    int wMask = level->width - 1;
    int dMask = level->depth - 1;

    // takes the first n entries off the front, new ones wait for the next round
    if (level->tickCount % 5 == 0) {
        int n = level->tickListSize;
        TickEntry* batch = (TickEntry*)malloc((size_t)(n > 0 ? n : 1) * sizeof(TickEntry));
        memcpy(batch, level->tickList, (size_t)n * sizeof(TickEntry));
        level->tickListSize = 0;
        for (int i = 0; i < n; ++i) {
            TickEntry e = batch[i];
            if (e.delay > 0) {
                e.delay--;
                if (level->tickListSize == level->tickListCapacity) {
                    level->tickListCapacity *= 2;
                    level->tickList = (TickEntry*)realloc(level->tickList, (size_t)level->tickListCapacity * sizeof(TickEntry));
                }
                level->tickList[level->tickListSize++] = e;
                continue;
            }
            if (e.x < 0 || e.y < 0 || e.z < 0 || e.x >= level->width || e.y >= level->depth || e.z >= level->height) continue;
            int id = level->blocks[(e.y * level->height + e.z) * level->width + e.x];
            if (id != e.id || id <= 0) continue;
            if (gTiles[id]->onTick) gTiles[id]->onTick(gTiles[id], level, e.x, e.y, e.z);
        }
        free(batch);
    }

    level->unprocessed += level->width * level->height * level->depth;
    int ticks = level->unprocessed / 200;
    level->unprocessed -= ticks * 200;

    // the tick lcg, overflowing like a 32 bit int
    for (int i = 0; i < ticks; ++i) {
        level->randValue = (int)((unsigned)level->randValue * 3u + 1013904223u);
        int r = level->randValue >> 2;
        int z = r >> wBits & hMask;
        int x = r & wMask;
        int y = r >> (wBits + hBits) & dMask;
        int id = level->blocks[(y * level->height + z) * level->width + x];
        if (gTiles[id] && gTiles[id]->shouldTick && gTiles[id]->onTick) gTiles[id]->onTick(gTiles[id], level, x, y, z);
    }
}
