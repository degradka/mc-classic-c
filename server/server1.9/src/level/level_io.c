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
    printf("Version is 1!\n");
    fflush(stdout);
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

static Level* loadV2(const uint8_t* p, size_t len) {
    JNode* root = NULL;
    JStream* s = JStream_read(p, len, &root);
    if (!s) {
        Log_stackTrace("java.io.StreamCorruptedException");
        return NULL;
    }
    char exception[256] = "";
    const JClass* c = root->cls;
    JNode* blocks = NULL;
    JNode* entities = NULL;
    if (!JNode_is(root, LV)) {
        snprintf(exception, sizeof exception, "java.lang.ClassCastException");
    } else if (c->suid != 0) {
        snprintf(exception, sizeof exception,
                 "java.io.InvalidClassException: local class incompatible: stream classdesc serialVersionUID = %lld, local class serialVersionUID = 0",
                 (long long)c->suid);
    } else if (!fieldType(c, "createTime", 'J', NULL) || !fieldType(c, "depth", 'I', NULL) || !fieldType(c, "height", 'I', NULL) ||
               !fieldType(c, "networkMode", 'Z', NULL) || !fieldType(c, "rotSpawn", 'F', NULL) || !fieldType(c, "tickCount", 'I', NULL) || !fieldType(c, "unprocessed", 'I', NULL) ||
               !fieldType(c, "width", 'I', NULL) || !fieldType(c, "xSpawn", 'I', NULL) || !fieldType(c, "ySpawn", 'I', NULL) ||
               !fieldType(c, "zSpawn", 'I', NULL) || !fieldType(c, "blocks", '[', "[B") ||
               !fieldType(c, "creator", 'L', "Ljava/lang/String;") || !fieldType(c, "entities", 'L', "Ljava/util/ArrayList;") ||
               !fieldType(c, "name", 'L', "Ljava/lang/String;")) {
        snprintf(exception, sizeof exception, "java.io.InvalidClassException: incompatible types for field");
    } else {
        blocks = JNode_obj(root, LV, "blocks");
        entities = JNode_obj(root, LV, "entities");
        if (entities && (entities->kind != JN_OBJECT || !JNode_is(entities, "java.util.ArrayList") || JNode_int(entities, NULL, "size") != 0)) {
            // the server can't hold entities, a client's mob class doesn't exist here
            snprintf(exception, sizeof exception, "java.lang.ClassNotFoundException");
        } else if (!blocks) {
            snprintf(exception, sizeof exception, "java.lang.RuntimeException: The level is corrupt!");
        }
    }
    int w = (int)JNode_int(root, LV, "width"), h = (int)JNode_int(root, LV, "height"), d = (int)JNode_int(root, LV, "depth");
    if (!exception[0] && (w < 0 || h < 0 || (long long)w * h < 0)) {
        snprintf(exception, sizeof exception, "java.lang.NegativeArraySizeException");
    }
    if (!exception[0] && blocks->len < (long long)w * h * d) {
        snprintf(exception, sizeof exception, "java.lang.ArrayIndexOutOfBoundsException: %d", (d - 1) * h * w);
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
    level->tickCount = (int)JNode_int(root, LV, "tickCount");
    level->unprocessed = (int)JNode_int(root, LV, "unprocessed");
    // ArrayList.readObject keeps the saved array length, a missing list gets a new one
    level->entitiesCapacity = entities && entities->hasBlockInt ? entities->blockInt : 10;
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
    { 'J', "createTime", NULL }, { 'I', "depth", NULL }, { 'I', "height", NULL }, { 'Z', "networkMode", NULL }, { 'F', "rotSpawn", NULL },
    { 'I', "tickCount", NULL }, { 'I', "unprocessed", NULL }, { 'I', "width", NULL }, { 'I', "xSpawn", NULL },
    { 'I', "ySpawn", NULL }, { 'I', "zSpawn", NULL }, { '[', "blocks", "[B" }, { 'L', "creator", "Ljava/lang/String;" },
    { 'L', "entities", "Ljava/util/ArrayList;" }, { 'L', "name", "Ljava/lang/String;" }
};
static const JDesc LEVEL_DESC = { LV, 0, 2, 15, LEVEL_FIELDS, NULL };
static const JField ARRAY_LIST_FIELDS[] = { { 'I', "size", NULL } };
static const JDesc ARRAY_LIST_DESC = { "java.util.ArrayList", 8683452581122892189LL, 3, 1, ARRAY_LIST_FIELDS, NULL };

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
    JW_i8(&w, level->createTime);
    JW_i4(&w, level->depth);
    JW_i4(&w, level->height);
    JW_u1(&w, level->networkMode ? 1 : 0);
    JW_f4(&w, level->rotSpawn);
    JW_i4(&w, level->tickCount);
    JW_i4(&w, level->unprocessed);
    JW_i4(&w, level->width);
    JW_i4(&w, level->xSpawn);
    JW_i4(&w, level->ySpawn);
    JW_i4(&w, level->zSpawn);
    JW_byteArray(&w, (const unsigned char*)level->blocks, level->width * level->height * level->depth);
    writeString(&w, level->creator);
    JW_object(&w, &ARRAY_LIST_DESC, &level->entitiesCapacity);
    JW_i4(&w, 0);
    // the array length, not the size
    unsigned char capacity[4] = { (unsigned char)(level->entitiesCapacity >> 24), (unsigned char)(level->entitiesCapacity >> 16),
                                  (unsigned char)(level->entitiesCapacity >> 8), (unsigned char)level->entitiesCapacity };
    JW_block(&w, capacity, 4);
    JW_endBlock(&w);
    writeString(&w, level->name);
    size_t n = 0;
    uint8_t* gz = jgzip_compress(w.buf, w.len, &n);
    fwrite(gz, 1, n, (FILE*)file);
    free(gz);
    JWriter_free(&w);
}
