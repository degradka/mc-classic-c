// level/level_io.c

#include "level_io.h"
#include "javaser.h"
#include "../jgzip.h"
#include "../log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC 656127880
#define LV "com.mojang.minecraft.level.Level"
#define BM "com.mojang.minecraft.level.BlockMap"
#define SL "com.mojang.minecraft.level.BlockMap$Slot"

static int32_t be(const uint8_t* p, int n) {
    uint32_t v = 0;
    for (int i = 0; i < n; ++i) v = (v << 8) | p[i];
    return (int32_t)v;
}

static bool readUtf(const uint8_t* p, size_t len, size_t* pos, JStr* out) {
    if (*pos + 2 > len) return false;
    size_t n = (size_t)(uint16_t)be(p + *pos, 2);
    *pos += 2;
    if (*pos + n > len) return false;
    jstr_fromUtf8(out, p + *pos, (int)n);
    *pos += n;
    return true;
}

static JStr* newJStr(const JStr* s) {
    JStr* copy = (JStr*)malloc(sizeof *copy);
    jstr_copy(copy, s);
    return copy;
}

static Level* loadV1(const uint8_t* p, size_t len) {
    size_t pos = 0;
    static JStr name, creator;
    if (!readUtf(p, len, &pos, &name) || !readUtf(p, len, &pos, &creator) || pos + 14 > len) {
        Log_stackTrace("java.io.EOFException");
        return NULL;
    }
    int64_t createTime = ((int64_t)be(p + pos, 4) << 32) | (uint32_t)be(p + pos + 4, 4);
    pos += 8;
    int w = (int16_t)be(p + pos, 2), h = (int16_t)be(p + pos + 2, 2), d = (int16_t)be(p + pos + 4, 2);
    pos += 6;
    long long total = (long long)w * h * d;
    if (total < 0) {
        Log_stackTrace("java.lang.NegativeArraySizeException");
        return NULL;
    }
    if (pos + (size_t)total > len) {
        Log_stackTrace("java.io.EOFException");
        return NULL;
    }
    int8_t* blocks = (int8_t*)malloc((size_t)total + 1);
    memcpy(blocks, p + pos, (size_t)total);
    Level* level = Level_create();
    Level_setData(level, w, d, h, blocks);
    level->name = newJStr(&name);
    level->creator = newJStr(&creator);
    level->createTime = createTime;
    return level;
}

static const JField* streamField(const JClass* c, const char* name) {
    for (int i = 0; i < c->nfields; ++i) if (strcmp(c->fields[i].name, name) == 0) return &c->fields[i];
    return NULL;
}

static bool fieldType(const JClass* c, const char* name, char type, const char* cls) {
    const JField* f = streamField(c, name);
    if (!f) return true;
    if (f->type != type) return false;
    return !cls || (f->cls && strcmp(f->cls, cls) == 0);
}

static void stringField(const JNode* root, const char* name, JStr** out) {
    JNode* n = JNode_obj(root, LV, name);
    if (!n || n->kind != JN_STRING || !n->str) return;
    JStr s;
    jstr_set(&s, n->str);
    *out = newJStr(&s);
}

// the server has no entity classes, anything outside these can't be read
static const char* const KNOWN[] = {
    LV, BM, SL, "java.util.ArrayList", "[Ljava.util.ArrayList;", "[Ljava.util.List;", "[B"
};

// the level's input stream looks these up as soon as their descriptor is read
static const char* const LOOKED_UP[] = {
    "com.mojang.minecraft.player.Player$1", "com.mojang.minecraft.mob.Creeper$1", "com.mojang.minecraft.mob.Skeleton$1"
};

typedef struct {
    const JNode* seen[4096];
    int nseen;
    bool unknown, lookedUp;
} Walk;

static bool named(const JNode* n, const char* const* names, size_t count) {
    for (size_t i = 0; i < count; ++i) if (n->cls && strcmp(n->cls->name, names[i]) == 0) return true;
    return false;
}

static void walk(Walk* w, const JNode* n) {
    if (!n || n->kind == JN_STRING || n->kind == JN_CLASS) return;
    for (int i = 0; i < w->nseen; ++i) if (w->seen[i] == n) return;
    if (w->nseen == 4096) return;
    w->seen[w->nseen++] = n;
    if (!named(n, KNOWN, sizeof KNOWN / sizeof *KNOWN)) w->unknown = true;
    if (named(n, LOOKED_UP, sizeof LOOKED_UP / sizeof *LOOKED_UP)) w->lookedUp = true;
    for (int i = 0; i < n->nvals; ++i) walk(w, n->vals[i].ref);
    for (int i = 0; i < n->nextra; ++i) walk(w, n->extra[i]);
    if (n->kind == JN_ARRAY && n->elems) for (int i = 0; i < n->len; ++i) walk(w, n->elems[i]);
}

// ArrayList.readObject keeps the saved array length, -1 stands for null
static int listCapacity(const JNode* n) {
    if (!n) return -1;
    return n->hasBlockInt ? n->blockInt : 0;
}

static BlockMap* readBlockMap(const JNode* n) {
    BlockMap* map = (BlockMap*)calloc(1, sizeof *map);
    map->width = (int)JNode_int(n, BM, "width");
    map->depth = (int)JNode_int(n, BM, "depth");
    map->height = (int)JNode_int(n, BM, "height");
    map->allCapacity = listCapacity(JNode_obj(n, BM, "all"));
    map->tmpCapacity = listCapacity(JNode_obj(n, BM, "tmp"));
    const JNode* grid = JNode_obj(n, BM, "entityGrid");
    map->cells = grid && grid->kind == JN_ARRAY ? grid->len : -1;
    if (map->cells > 0) {
        map->cellCapacity = (int*)malloc((size_t)map->cells * sizeof(int));
        for (int i = 0; i < map->cells; ++i) map->cellCapacity[i] = listCapacity(grid->elems ? grid->elems[i] : NULL);
    }
    const char* slots[2] = { "slot", "slot2" };
    for (int i = 0; i < 2; ++i) {
        const JNode* slot = JNode_obj(n, BM, slots[i]);
        if (!slot) {
            map->slot[i][0] = INT32_MIN;
            continue;
        }
        map->slot[i][0] = (int)JNode_int(slot, SL, "xSlot");
        map->slot[i][1] = (int)JNode_int(slot, SL, "ySlot");
        map->slot[i][2] = (int)JNode_int(slot, SL, "zSlot");
    }
    return map;
}

static Level* loadV2(const uint8_t* p, size_t len) {
    JNode* root = NULL;
    JStream* s = JStream_read(p, len, &root);
    if (!s) {
        Log_stackTrace("java.io.StreamCorruptedException");
        return NULL;
    }
    char exception[256] = "";
    const JClass* c = root ? root->cls : NULL;
    JNode* blocks = NULL;
    static Walk walked;
    memset(&walked, 0, sizeof walked);
    walk(&walked, root);
    bool isLevel = root && JNode_is(root, LV);
    if (!root) {
        snprintf(exception, sizeof exception, "java.lang.NullPointerException");
    } else if (isLevel && c->suid != 0) {
        snprintf(exception, sizeof exception,
                 "java.io.InvalidClassException: local class incompatible: stream classdesc serialVersionUID = %lld, local class serialVersionUID = 0",
                 (long long)c->suid);
    } else if (isLevel && (!fieldType(c, "cloudColor", 'I', NULL) || !fieldType(c, "createTime", 'J', NULL) || !fieldType(c, "creativeMode", 'Z', NULL) ||
               !fieldType(c, "depth", 'I', NULL) || !fieldType(c, "fogColor", 'I', NULL) || !fieldType(c, "height", 'I', NULL) ||
               !fieldType(c, "networkMode", 'Z', NULL) || !fieldType(c, "rotSpawn", 'F', NULL) || !fieldType(c, "skyColor", 'I', NULL) ||
               !fieldType(c, "tickCount", 'I', NULL) || !fieldType(c, "unprocessed", 'I', NULL) || !fieldType(c, "waterLevel", 'I', NULL) ||
               !fieldType(c, "width", 'I', NULL) || !fieldType(c, "xSpawn", 'I', NULL) || !fieldType(c, "ySpawn", 'I', NULL) ||
               !fieldType(c, "zSpawn", 'I', NULL) || !fieldType(c, "blockMap", 'L', "Lcom/mojang/minecraft/level/BlockMap;") ||
               !fieldType(c, "blocks", '[', "[B") || !fieldType(c, "creator", 'L', "Ljava/lang/String;") ||
               !fieldType(c, "name", 'L', "Ljava/lang/String;") || !fieldType(c, "player", 'L', "Lcom/mojang/minecraft/Entity;"))) {
        snprintf(exception, sizeof exception, "java.io.InvalidClassException: incompatible types for field");
    } else if (walked.lookedUp) {
        snprintf(exception, sizeof exception, "java.io.InvalidClassException: failed to read class descriptor");
    } else if (walked.unknown) {
        snprintf(exception, sizeof exception, "java.lang.ClassNotFoundException");
    } else if (!isLevel) {
        snprintf(exception, sizeof exception, "java.lang.ClassCastException");
    } else {
        blocks = JNode_obj(root, LV, "blocks");
        if (!blocks) snprintf(exception, sizeof exception, "java.lang.RuntimeException: The level is corrupt!");
    }
    int w = 0, h = 0, d = 0;
    if (!exception[0]) {
        w = (int)JNode_int(root, LV, "width");
        h = (int)JNode_int(root, LV, "height");
        d = (int)JNode_int(root, LV, "depth");
        if (w < 0 || h < 0 || (long long)w * h < 0) {
            snprintf(exception, sizeof exception, "java.lang.NegativeArraySizeException");
        } else if (blocks->len < (long long)w * h * d) {
            snprintf(exception, sizeof exception, "java.lang.ArrayIndexOutOfBoundsException: %d", (d - 1) * h * w);
        }
    }
    if (exception[0]) {
        Log_stackTrace(exception);
        JStream_free(s);
        return NULL;
    }
    Level* level = (Level*)calloc(1, sizeof *level);
    level->width = w;
    level->height = h;
    level->depth = d;
    level->blocks = (int8_t*)malloc((size_t)blocks->len + 1);
    memcpy(level->blocks, blocks->bytes, (size_t)blocks->len);
    stringField(root, "name", &level->name);
    stringField(root, "creator", &level->creator);
    level->createTime = JNode_int(root, LV, "createTime");
    level->xSpawn = (int)JNode_int(root, LV, "xSpawn");
    level->ySpawn = (int)JNode_int(root, LV, "ySpawn");
    level->zSpawn = (int)JNode_int(root, LV, "zSpawn");
    level->rotSpawn = JNode_float(root, LV, "rotSpawn");
    level->networkMode = JNode_int(root, LV, "networkMode") != 0;
    level->creativeMode = JNode_int(root, LV, "creativeMode") != 0;
    level->waterLevel = (int)JNode_int(root, LV, "waterLevel");
    level->skyColor = (int)JNode_int(root, LV, "skyColor");
    level->fogColor = (int)JNode_int(root, LV, "fogColor");
    level->cloudColor = (int)JNode_int(root, LV, "cloudColor");
    level->tickCount = (int)JNode_int(root, LV, "tickCount");
    level->unprocessed = (int)JNode_int(root, LV, "unprocessed");
    const JNode* map = JNode_obj(root, LV, "blockMap");
    if (map) level->blockMap = readBlockMap(map);
    JStream_free(s);
    Level_initTransient(level);
    return level;
}

Level* LevelIO_load(const uint8_t* data, int len) {
    Log_info("Loading level");
    char exception[256];
    size_t n = 0;
    uint8_t* raw = jgzip_decompress(data, (size_t)len, &n, exception, sizeof exception);
    if (!raw) {
        Log_stackTrace(exception);
        return NULL;
    }
    Level* level = NULL;
    if (n < 5) {
        Log_stackTrace("java.io.EOFException");
    } else if (be(raw, 4) == MAGIC) {
        int8_t version = (int8_t)raw[4];
        if (version <= 1) level = loadV1(raw + 5, n - 5);
        else if (version == 2) level = loadV2(raw + 5, n - 5);
    }
    free(raw);
    return level;
}

/* save */

static const JField LEVEL_FIELDS[] = {
    { 'I', "cloudColor", NULL }, { 'J', "createTime", NULL }, { 'Z', "creativeMode", NULL }, { 'I', "depth", NULL },
    { 'I', "fogColor", NULL }, { 'I', "height", NULL }, { 'Z', "networkMode", NULL }, { 'F', "rotSpawn", NULL },
    { 'I', "skyColor", NULL }, { 'I', "tickCount", NULL }, { 'I', "unprocessed", NULL }, { 'I', "waterLevel", NULL },
    { 'I', "width", NULL }, { 'I', "xSpawn", NULL }, { 'I', "ySpawn", NULL }, { 'I', "zSpawn", NULL },
    { 'L', "blockMap", "Lcom/mojang/minecraft/level/BlockMap;" }, { '[', "blocks", "[B" },
    { 'L', "creator", "Ljava/lang/String;" }, { 'L', "name", "Ljava/lang/String;" },
    { 'L', "player", "Lcom/mojang/minecraft/Entity;" }
};
static const JDesc LEVEL_DESC = { LV, 0, 2, 21, LEVEL_FIELDS, NULL };
static const JField BLOCK_MAP_FIELDS[] = {
    { 'I', "depth", NULL }, { 'I', "height", NULL }, { 'I', "width", NULL }, { 'L', "all", "Ljava/util/List;" },
    { '[', "entityGrid", "[Ljava/util/List;" }, { 'L', "slot", "Lcom/mojang/minecraft/level/BlockMap$Slot;" },
    { 'L', "slot2", "Lcom/mojang/minecraft/level/BlockMap$Slot;" }, { 'L', "tmp", "Ljava/util/List;" }
};
static const JDesc BLOCK_MAP_DESC = { BM, 0, 2, 8, BLOCK_MAP_FIELDS, NULL };
static const JField SLOT_FIELDS[] = {
    { 'I', "xSlot", NULL }, { 'I', "ySlot", NULL }, { 'I', "zSlot", NULL }, { 'L', "this$0", "Lcom/mojang/minecraft/level/BlockMap;" }
};
static const JDesc SLOT_DESC = { SL, 0, 2, 4, SLOT_FIELDS, NULL };
static const JField ARRAY_LIST_FIELDS[] = { { 'I', "size", NULL } };
static const JDesc ARRAY_LIST_DESC = { "java.util.ArrayList", 8683452581122892189LL, 3, 1, ARRAY_LIST_FIELDS, NULL };
static const JDesc ARRAY_LIST_ARRAY_DESC = { "[Ljava.util.ArrayList;", 8686596894184378438LL, 2, 0, NULL, NULL };

// an empty list, the array length goes in its block data
static void writeList(JWriter* w, const int* capacity) {
    if (*capacity < 0) {
        JW_null(w);
        return;
    }
    JW_object(w, &ARRAY_LIST_DESC, capacity);
    JW_i4(w, 0);
    unsigned char b[4] = { (unsigned char)(*capacity >> 24), (unsigned char)(*capacity >> 16), (unsigned char)(*capacity >> 8), (unsigned char)*capacity };
    JW_block(w, b, 4);
    JW_endBlock(w);
}

static void writeSlot(JWriter* w, const BlockMap* map, int i) {
    if (map->slot[i][0] == INT32_MIN) {
        JW_null(w);
        return;
    }
    JW_object(w, &SLOT_DESC, map->slot[i]);
    JW_i4(w, map->slot[i][0]);
    JW_i4(w, map->slot[i][1]);
    JW_i4(w, map->slot[i][2]);
    JW_ref(w, map);
}

static void writeBlockMap(JWriter* w, const BlockMap* map) {
    if (!map) {
        JW_null(w);
        return;
    }
    JW_object(w, &BLOCK_MAP_DESC, map);
    JW_i4(w, map->depth);
    JW_i4(w, map->height);
    JW_i4(w, map->width);
    writeList(w, &map->allCapacity);
    if (map->cells < 0) {
        JW_null(w);
    } else {
        JW_array(w, &ARRAY_LIST_ARRAY_DESC, &map->cells, map->cells);
        for (int i = 0; i < map->cells; ++i) writeList(w, &map->cellCapacity[i]);
    }
    writeSlot(w, map, 0);
    writeSlot(w, map, 1);
    writeList(w, &map->tmpCapacity);
}

static void writeString(JWriter* w, const JStr* s) {
    if (!s) {
        JW_null(w);
        return;
    }
    static unsigned char utf[JSTR_MAX * 3];
    int n = 0;
    for (int i = 0; i < s->n; ++i) {
        int c = s->c[i];
        if (c >= 1 && c < 0x80) {
            utf[n++] = (unsigned char)c;
        } else if (c < 0x800) {
            utf[n++] = (unsigned char)(0xC0 | (c >> 6));
            utf[n++] = (unsigned char)(0x80 | (c & 0x3F));
        } else {
            utf[n++] = (unsigned char)(0xE0 | (c >> 12));
            utf[n++] = (unsigned char)(0x80 | ((c >> 6) & 0x3F));
            utf[n++] = (unsigned char)(0x80 | (c & 0x3F));
        }
    }
    JW_newString(w, utf, n);
}

void LevelIO_save(const Level* level, void* file) {
    // DataOutputStream's magic and version go in front of the object stream
    JWriter w;
    memset(&w, 0, sizeof w);
    JW_i4(&w, MAGIC);
    JW_u1(&w, 2);
    JW_i2(&w, 0xACED);
    JW_i2(&w, 5);
    JW_object(&w, &LEVEL_DESC, level);
    JW_i4(&w, level->cloudColor);
    JW_i8(&w, level->createTime);
    JW_u1(&w, level->creativeMode ? 1 : 0);
    JW_i4(&w, level->depth);
    JW_i4(&w, level->fogColor);
    JW_i4(&w, level->height);
    JW_u1(&w, level->networkMode ? 1 : 0);
    JW_f4(&w, level->rotSpawn);
    JW_i4(&w, level->skyColor);
    JW_i4(&w, level->tickCount);
    JW_i4(&w, level->unprocessed);
    JW_i4(&w, level->waterLevel);
    JW_i4(&w, level->width);
    JW_i4(&w, level->xSpawn);
    JW_i4(&w, level->ySpawn);
    JW_i4(&w, level->zSpawn);
    writeBlockMap(&w, level->blockMap);
    JW_byteArray(&w, (const unsigned char*)level->blocks, level->width * level->height * level->depth);
    writeString(&w, level->creator);
    writeString(&w, level->name);
    JW_null(&w);
    size_t n = 0;
    uint8_t* gz = jgzip_compress(w.buf, w.len, &n);
    fwrite(gz, 1, n, (FILE*)file);
    free(gz);
    JWriter_free(&w);
}
