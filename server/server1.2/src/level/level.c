// level.c: world storage, lighting columns, IO, and solid cube queries

#include "level.h"
#include "tile/tile.h"
#include "levelgen/level_gen.h"
#include "../log.h"

#include <zlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

void Level_init(Level* level, int width, int height, int depth) {
    level->width = width;
    level->height = height;
    level->depth = depth;
    level->listener = NULL;
    level->listenerCtx = NULL;
    level->unprocessed = 0;
    level->xSpawn = level->ySpawn = level->zSpawn = 0;
    level->rotSpawn = 0.0f;
    level->tickRandom = (unsigned int)rand();
    level->tickCount = 0;
    level->tickList = NULL;
    level->tickListSize = 0;
    level->tickListCapacity = 0;

    level->blocks = (byte*)malloc((size_t)width * height * depth);
    level->lightDepths = (int*)malloc((size_t)width * height * sizeof(int));
    if (!level->blocks || !level->lightDepths) {
        Log_severe("Failed to allocate level memory");
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
    LevelBlockChangeListener listener = level->listener;
    void* listenerCtx = level->listenerCtx;
    level->listener = NULL; // detached during regeneration, restored below

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

    level->blocks = (byte*)malloc((size_t)width * height * depth);
    level->lightDepths = (int*)malloc((size_t)width * height * sizeof(int));
    if (!level->blocks || !level->lightDepths) {
        Log_severe("Failed to allocate level memory");
        exit(EXIT_FAILURE);
    }

    Level_generateMap(level);
    calcLightDepths(level, 0, 0, width, height);
    Level_findSpawn(level);

    level->listener = listener;
    level->listenerCtx = listenerCtx;
}

void Level_setListener(Level* level, LevelBlockChangeListener fn, void* ctx) {
    level->listener = fn;
    level->listenerCtx = ctx;
}

void calcLightDepths(Level* level, int minX, int minZ, int maxX, int maxZ) {
    // the client also compares against the previous depth here to notify its
    // renderer of a chunk mesh rebuild; the server has no mesh, so it doesn't
    for (int x = minX; x < minX + maxX; x++) {
        for (int z = minZ; z < minZ + maxZ; z++) {
            int d = level->depth - 1;
            while (d > 0 && !Level_isLightBlocker(level, x, d, z)) d--;
            level->lightDepths[x + z * level->width] = d + 1;
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

// server_level.dat is gzipped: a 4 byte magic, a version byte, then a real
// Java serialized Level object (the server always writes version 2, unlike
// the client's simpler hand rolled version 1 level.dat -- these are NOT the
// same format despite sharing the magic/version wrapper convention). Byte
// layout confirmed against a real save file field by field; see
// PORTING_SCOPE.md for the full derivation. The class descriptor is a fixed
// byte sequence (deterministic for this exact, unchanging field layout), so
// it's just a hardcoded template rather than something built field by field
// at runtime -- both for writing, and as a known-length block to validate
// and skip over when reading.

// magic + version byte + Java serialization stream header (0xACED0005) +
// full class descriptor for com.mojang.minecraft.level.Level (14 fields, in
// Java's actual default serialization order: primitives alphabetically,
// then objects alphabetically -- NOT declaration order): createTime(J),
// depth(I), height(I), rotSpawn(F), tickCount(I), unprocessed(I), width(I),
// xSpawn(I), ySpawn(I), zSpawn(I), blocks([B), creator(Ljava/lang/String;),
// entities(Ljava/util/ArrayList;), name(Ljava/lang/String;)
static const unsigned char LEVEL_HEADER_TEMPLATE[252] = {
    0x27, 0x1b, 0xb7, 0x88, 0x02, 0xac, 0xed, 0x00, 0x05, 0x73, 0x72, 0x00,
    0x20, 0x63, 0x6f, 0x6d, 0x2e, 0x6d, 0x6f, 0x6a, 0x61, 0x6e, 0x67, 0x2e,
    0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x2e, 0x6c, 0x65,
    0x76, 0x65, 0x6c, 0x2e, 0x4c, 0x65, 0x76, 0x65, 0x6c, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x0e, 0x4a, 0x00, 0x0a, 0x63,
    0x72, 0x65, 0x61, 0x74, 0x65, 0x54, 0x69, 0x6d, 0x65, 0x49, 0x00, 0x05,
    0x64, 0x65, 0x70, 0x74, 0x68, 0x49, 0x00, 0x06, 0x68, 0x65, 0x69, 0x67,
    0x68, 0x74, 0x46, 0x00, 0x08, 0x72, 0x6f, 0x74, 0x53, 0x70, 0x61, 0x77,
    0x6e, 0x49, 0x00, 0x09, 0x74, 0x69, 0x63, 0x6b, 0x43, 0x6f, 0x75, 0x6e,
    0x74, 0x49, 0x00, 0x0b, 0x75, 0x6e, 0x70, 0x72, 0x6f, 0x63, 0x65, 0x73,
    0x73, 0x65, 0x64, 0x49, 0x00, 0x05, 0x77, 0x69, 0x64, 0x74, 0x68, 0x49,
    0x00, 0x06, 0x78, 0x53, 0x70, 0x61, 0x77, 0x6e, 0x49, 0x00, 0x06, 0x79,
    0x53, 0x70, 0x61, 0x77, 0x6e, 0x49, 0x00, 0x06, 0x7a, 0x53, 0x70, 0x61,
    0x77, 0x6e, 0x5b, 0x00, 0x06, 0x62, 0x6c, 0x6f, 0x63, 0x6b, 0x73, 0x74,
    0x00, 0x02, 0x5b, 0x42, 0x4c, 0x00, 0x07, 0x63, 0x72, 0x65, 0x61, 0x74,
    0x6f, 0x72, 0x74, 0x00, 0x12, 0x4c, 0x6a, 0x61, 0x76, 0x61, 0x2f, 0x6c,
    0x61, 0x6e, 0x67, 0x2f, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x3b, 0x4c,
    0x00, 0x08, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x69, 0x65, 0x73, 0x74, 0x00,
    0x15, 0x4c, 0x6a, 0x61, 0x76, 0x61, 0x2f, 0x75, 0x74, 0x69, 0x6c, 0x2f,
    0x41, 0x72, 0x72, 0x61, 0x79, 0x4c, 0x69, 0x73, 0x74, 0x3b, 0x4c, 0x00,
    0x04, 0x6e, 0x61, 0x6d, 0x65, 0x71, 0x00, 0x7e, 0x00, 0x02, 0x78, 0x70
};

// the `blocks` field's array VALUE (not its type descriptor, which is
// already in the header above): TC_ARRAY, nested class descriptor for the
// intrinsic byte[] ("[B") type, then immediately followed on the wire by a
// 4 byte big endian element count and the raw bytes (written/read separately
// below, not part of this fixed template)
static const unsigned char BLOCKS_ARRAY_HEADER[19] = {
    0x75, 0x72, 0x00, 0x02, 0x5b, 0x42, 0xac, 0xf3, 0x17, 0xf8, 0x06, 0x08,
    0x54, 0xe0, 0x02, 0x00, 0x00, 0x78, 0x70
};

// the `entities` field's ArrayList VALUE when empty: TC_OBJECT + full class
// descriptor for java.util.ArrayList (which has a custom writeObject, hence
// the SC_WRITE_METHOD flag and the block data after its one declared `size`
// field), size=0, and an empty custom-written element block. Always written
// this way since nothing in this port stores entities on Level itself
// (mobs/players are tracked separately, matching how the client's own Level
// has no such field at all)
static const unsigned char EMPTY_ENTITIES_TEMPLATE[54] = {
    0x73, 0x72, 0x00, 0x13, 0x6a, 0x61, 0x76, 0x61, 0x2e, 0x75, 0x74, 0x69,
    0x6c, 0x2e, 0x41, 0x72, 0x72, 0x61, 0x79, 0x4c, 0x69, 0x73, 0x74, 0x78,
    0x81, 0xd2, 0x1d, 0x99, 0xc7, 0x61, 0x9d, 0x03, 0x00, 0x01, 0x49, 0x00,
    0x04, 0x73, 0x69, 0x7a, 0x65, 0x78, 0x70, 0x00, 0x00, 0x00, 0x00, 0x77,
    0x04, 0x00, 0x00, 0x00, 0x00, 0x78
};

static void writeJavaInt(gzFile f, int v) {
    unsigned char b[4] = { (unsigned char)(v >> 24), (unsigned char)(v >> 16),
                            (unsigned char)(v >> 8),  (unsigned char)v };
    gzwrite(f, b, 4);
}

static int readJavaInt(gzFile f, int* out) {
    unsigned char b[4];
    if (gzread(f, b, 4) != 4) return 0;
    *out = ((int)b[0] << 24) | ((int)b[1] << 16) | ((int)b[2] << 8) | (int)b[3];
    return 1;
}

static void writeJavaLong(gzFile f, long long v) {
    unsigned char b[8];
    for (int i = 0; i < 8; ++i) b[i] = (unsigned char)(v >> (56 - i * 8));
    gzwrite(f, b, 8);
}

static int readJavaLong(gzFile f, long long* out) {
    unsigned char b[8];
    if (gzread(f, b, 8) != 8) return 0;
    long long v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | b[i];
    *out = v;
    return 1;
}

static void writeJavaFloat(gzFile f, float v) {
    unsigned int bits;
    memcpy(&bits, &v, sizeof bits);
    writeJavaInt(f, (int)bits);
}

static int readJavaFloat(gzFile f, float* out) {
    int bits;
    if (!readJavaInt(f, &bits)) return 0;
    unsigned int ubits = (unsigned int)bits;
    memcpy(out, &ubits, sizeof ubits);
    return 1;
}

// TC_STRING: 1 byte tag (0x74), 2 byte big endian length, then that many
// modified UTF-8 bytes. Plain ASCII strings (all this port ever writes) are
// valid modified UTF-8 as-is, no special encoding needed
static void writeJavaString(gzFile f, const char* s) {
    unsigned char tag = 0x74;
    gzwrite(f, &tag, 1);
    size_t len = strlen(s);
    unsigned char lenBytes[2] = { (unsigned char)(len >> 8), (unsigned char)len };
    gzwrite(f, lenBytes, 2);
    gzwrite(f, s, (unsigned)len);
}

// handles TC_STRING normally. A field VALUE could in principle also be a
// TC_REFERENCE (0x71 + 4 byte handle) if it happens to alias an earlier
// string object, but that's a runtime object identity coincidence that
// practically never happens for independent name/creator strings (unlike
// the header's fixed type descriptor strings, which do alias each other and
// are already baked into the hardcoded template above) -- falls back to an
// empty string rather than actually resolving the handle table in that case
static int readJavaString(gzFile f, char* out, size_t outCapacity) {
    unsigned char tag;
    if (gzread(f, &tag, 1) != 1) return 0;
    if (tag == 0x71) { // TC_REFERENCE, see comment above
        unsigned char handle[4];
        if (gzread(f, handle, 4) != 4) return 0;
        out[0] = '\0';
        return 1;
    }
    if (tag != 0x74) return 0;
    unsigned char lenBytes[2];
    if (gzread(f, lenBytes, 2) != 2) return 0;
    size_t len = ((size_t)lenBytes[0] << 8) | lenBytes[1];
    size_t toCopy = (len < outCapacity - 1) ? len : outCapacity - 1;
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

bool Level_load(Level* level) {
    gzFile f = gzopen("server_level.dat", "rb");
    if (!f) return false;

    unsigned char header[sizeof LEVEL_HEADER_TEMPLATE];
    if (gzread(f, header, sizeof header) != (int)sizeof header ||
        memcmp(header, LEVEL_HEADER_TEMPLATE, sizeof header) != 0) {
        gzclose(f);
        return false;
    }

    long long createTime;
    int depth, height, tickCount, unprocessed, width, xSpawn, ySpawn, zSpawn;
    float rotSpawn;
    if (!readJavaLong(f, &createTime)  || !readJavaInt(f, &depth)   ||
        !readJavaInt(f, &height)       || !readJavaFloat(f, &rotSpawn) ||
        !readJavaInt(f, &tickCount)    || !readJavaInt(f, &unprocessed) ||
        !readJavaInt(f, &width)        || !readJavaInt(f, &xSpawn)  ||
        !readJavaInt(f, &ySpawn)       || !readJavaInt(f, &zSpawn)) {
        gzclose(f);
        return false;
    }

    unsigned char blocksHeader[sizeof BLOCKS_ARRAY_HEADER];
    int blockCount;
    if (gzread(f, blocksHeader, sizeof blocksHeader) != (int)sizeof blocksHeader ||
        memcmp(blocksHeader, BLOCKS_ARRAY_HEADER, sizeof blocksHeader) != 0 ||
        !readJavaInt(f, &blockCount) || blockCount != width * height * depth) {
        gzclose(f);
        return false;
    }

    size_t total = (size_t)blockCount;
    byte* blocks = (byte*)malloc(total);
    if (!blocks || gzread(f, blocks, (unsigned)total) != (int)total) {
        free(blocks);
        gzclose(f);
        return false;
    }

    char creator[64], name[64];
    if (!readJavaString(f, creator, sizeof creator)) {
        free(blocks); gzclose(f); return false;
    }

    // entities: skip past the ArrayList object rather than assuming it's
    // exactly the empty template, so a real save with actual saved entities
    // still parses correctly (their contents are just not imported, nothing
    // in this port reads Level.entities for anything)
    unsigned char entitiesTag;
    if (gzread(f, &entitiesTag, 1) != 1 || entitiesTag != 0x73) { free(blocks); gzclose(f); return false; }
    // ArrayList's class descriptor (TC_CLASSDESC through TC_NULL), the tag
    // byte just read excluded: 42 bytes, verified directly against
    // EMPTY_ENTITIES_TEMPLATE's own layout, not a derived/computed size
    unsigned char skipBuf[42];
    if (gzread(f, skipBuf, sizeof skipBuf) != (int)sizeof skipBuf) { free(blocks); gzclose(f); return false; }
    int entitySize;
    if (!readJavaInt(f, &entitySize)) { free(blocks); gzclose(f); return false; }
    (void)entitySize; // consumed to advance the stream, not stored anywhere

    // only handles the short block form (TC_BLOCKDATASHORT, <256 bytes of
    // element data) since a save with actual entities is not a case this
    // port produces or needs to import; the long form (TC_BLOCKDATA, 0x7A)
    // would appear here instead for a real save with enough entities to
    // exceed that, and isn't handled
    unsigned char blockTag, blockLen;
    if (gzread(f, &blockTag, 1) != 1 || blockTag != 0x77 || gzread(f, &blockLen, 1) != 1) {
        free(blocks); gzclose(f); return false;
    }
    char blockDiscard[256];
    if (blockLen > 0 && gzread(f, blockDiscard, blockLen) != blockLen) { free(blocks); gzclose(f); return false; }
    unsigned char endTag;
    if (gzread(f, &endTag, 1) != 1 || endTag != 0x78) { free(blocks); gzclose(f); return false; }

    if (!readJavaString(f, name, sizeof name)) {
        free(blocks); gzclose(f); return false;
    }
    gzclose(f);

    free(level->blocks);
    free(level->lightDepths);

    level->width = width; level->height = height; level->depth = depth;
    level->blocks = blocks;
    memcpy(level->name, name, sizeof(level->name));
    memcpy(level->creator, creator, sizeof(level->creator));
    level->createTime = createTime;
    level->xSpawn = xSpawn; level->ySpawn = ySpawn; level->zSpawn = zSpawn;
    level->rotSpawn = rotSpawn;
    level->tickCount = tickCount;
    level->unprocessed = unprocessed;

    level->lightDepths = (int*)malloc((size_t)width * height * sizeof(int));
    if (!level->lightDepths) {
        Log_severe("Failed to allocate level memory");
        exit(EXIT_FAILURE);
    }

    return true;
}

void Level_save(const Level* level) {
    gzFile f = gzopen("server_level.dat", "wb");
    if (!f) return;

    gzwrite(f, LEVEL_HEADER_TEMPLATE, sizeof LEVEL_HEADER_TEMPLATE);

    writeJavaLong(f, level->createTime);
    writeJavaInt(f, level->depth);
    writeJavaInt(f, level->height);
    writeJavaFloat(f, level->rotSpawn);
    writeJavaInt(f, level->tickCount);
    writeJavaInt(f, level->unprocessed);
    writeJavaInt(f, level->width);
    writeJavaInt(f, level->xSpawn);
    writeJavaInt(f, level->ySpawn);
    writeJavaInt(f, level->zSpawn);

    gzwrite(f, BLOCKS_ARRAY_HEADER, sizeof BLOCKS_ARRAY_HEADER);
    size_t total = (size_t)level->width * level->height * level->depth;
    writeJavaInt(f, (int)total);
    gzwrite(f, level->blocks, (unsigned)total);

    writeJavaString(f, level->creator);
    gzwrite(f, EMPTY_ENTITIES_TEMPLATE, sizeof EMPTY_ENTITIES_TEMPLATE);
    writeJavaString(f, level->name);

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
    if (level->listener) level->listener(level->listenerCtx, x, y, z);
    return true;
}

// No neighbor notification, no light recalc. Used by liquid tick reactions
// to avoid cascading recursion, matching Java exactly -- but the listener
// still fires, since a connected client needs to see this change too even
// when it doesn't cascade locally (e.g. liquid meeting liquid turning to
// rock), confirmed against the real server's setTileNoNeighborChange
bool Level_setTileNoUpdate(Level* level, int x, int y, int z, int type) {
    if (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height) return false;
    int index = (y * level->height + z) * level->width + x;
    if (level->blocks[index] == (byte)type) return false;
    level->blocks[index] = (byte)type;
    if (level->listener) level->listener(level->listenerCtx, x, y, z);
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

float Level_getBrightness(const Level* level, int x, int y, int z) {
    // matches the paired c0.0.16a_02 client's real value, confirmed directly
    // against server1.2's own decompiled Level.getBrightness
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
    if (level->listener) {
        level->listener(level->listenerCtx, x1, y1, z1);
        level->listener(level->listenerCtx, x2, y2, z2);
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