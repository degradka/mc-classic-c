// level/level_io.c: level.dat version 2, a serialized Level with its zombies

#include "level_io.h"
#include "javaser.h"
#include "../jgzip.h"
#include "../character/zombie.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC 656127880
#define LV "com.mojang.minecraft.level.Level"
#define EN "com.mojang.minecraft.Entity"
#define ZB "com.mojang.minecraft.character.Zombie"

extern void Minecraft_beginLevelLoading(const char* title);
extern void Minecraft_levelLoadUpdate(const char* status);

static const JField LEVEL_FIELDS[] = {
    { 'J', "createTime", NULL }, { 'I', "depth", NULL }, { 'I', "height", NULL },
    { 'F', "rotSpawn", NULL }, { 'I', "tickCount", NULL }, { 'I', "unprocessed", NULL }, { 'I', "width", NULL },
    { 'I', "xSpawn", NULL }, { 'I', "ySpawn", NULL }, { 'I', "zSpawn", NULL }, { '[', "blocks", "[B" },
    { 'L', "creator", "Ljava/lang/String;" }, { 'L', "entities", "Ljava/util/ArrayList;" }, { 'L', "name", "Ljava/lang/String;" }
};
static const JDesc LEVEL_DESC = { LV, 0, 2, 14, LEVEL_FIELDS, NULL };
static const JField LIST_FIELDS[] = { { 'I', "size", NULL } };
static const JDesc LIST_DESC = { "java.util.ArrayList", 8683452581122892189LL, 3, 1, LIST_FIELDS, NULL };
static const JField ENTITY_FIELDS[] = {
    { 'F', "bbHeight", NULL }, { 'F', "bbWidth", NULL }, { 'F', "heightOffset", NULL }, { 'Z', "horizontalCollision", NULL },
    { 'Z', "onGround", NULL }, { 'Z', "removed", NULL },
    { 'F', "x", NULL }, { 'F', "xRot", NULL }, { 'F', "xRotO", NULL }, { 'F', "xd", NULL }, { 'F', "xo", NULL },
    { 'F', "y", NULL }, { 'F', "yRot", NULL }, { 'F', "yRotO", NULL }, { 'F', "yd", NULL }, { 'F', "yo", NULL },
    { 'F', "z", NULL }, { 'F', "zd", NULL }, { 'F', "zo", NULL },
    { 'L', "bb", "Lcom/mojang/minecraft/phys/AABB;" }, { 'L', "level", "Lcom/mojang/minecraft/level/Level;" }
};
static const JDesc ENTITY_DESC = { EN, 0, 2, 21, ENTITY_FIELDS, NULL };
static const JField ZOMBIE_FIELDS[] = { { 'F', "rot", NULL }, { 'F', "rotA", NULL }, { 'F', "speed", NULL }, { 'F', "timeOffs", NULL } };
static const JDesc ZOMBIE_DESC = { ZB, 77479605454997290LL, 2, 4, ZOMBIE_FIELDS, &ENTITY_DESC };
static const JField AABB_FIELDS[] = {
    { 'F', "epsilon", NULL }, { 'F', "x0", NULL }, { 'F', "x1", NULL }, { 'F', "y0", NULL }, { 'F', "y1", NULL },
    { 'F', "z0", NULL }, { 'F', "z1", NULL }
};
static const JDesc AABB_DESC = { "com.mojang.minecraft.phys.AABB", 0, 2, 7, AABB_FIELDS, NULL };

/* save */

// a server's level has no name or creator, they're empty and saved as null
static void writeString(JWriter* w, const char* s) {
    if (!s[0]) {
        JW_null(w);
        return;
    }
    JW_newString(w, (const unsigned char*)s, (int)strlen(s));
}

static void writeZombie(JWriter* w, const Zombie* z, const Level* level) {
    const Entity* e = &z->base;
    JW_object(w, &ZOMBIE_DESC, z);
    JW_f4(w, e->boundingBoxHeight);
    JW_f4(w, e->boundingBoxWidth);
    JW_f4(w, e->heightOffset);
    JW_u1(w, e->horizontalCollision);
    JW_u1(w, e->onGround);
    JW_u1(w, e->removed);
    JW_f4(w, e->x);
    JW_f4(w, e->xRotation);
    JW_f4(w, e->xRotO);
    JW_f4(w, e->motionX);
    JW_f4(w, e->prevX);
    JW_f4(w, e->y);
    JW_f4(w, e->yRotation);
    JW_f4(w, e->yRotO);
    JW_f4(w, e->motionY);
    JW_f4(w, e->prevY);
    JW_f4(w, e->z);
    JW_f4(w, e->motionZ);
    JW_f4(w, e->prevZ);
    JW_object(w, &AABB_DESC, &e->boundingBox);
    JW_f4(w, 0.0f);
    JW_f4(w, e->boundingBox.minX);
    JW_f4(w, e->boundingBox.maxX);
    JW_f4(w, e->boundingBox.minY);
    JW_f4(w, e->boundingBox.maxY);
    JW_f4(w, e->boundingBox.minZ);
    JW_f4(w, e->boundingBox.maxZ);
    JW_ref(w, level);
    JW_f4(w, z->rotation);
    JW_f4(w, z->rotationMotionFactor);
    JW_f4(w, z->speed);
    JW_f4(w, z->timeOffset);
}

// stops before the first network player, it can't be serialized and the gzip is left unfinished
static bool writeLevel(JWriter* w, const Level* level) {
    JW_i4(w, MAGIC);
    JW_u1(w, 2);
    JW_i2(w, 0xACED);
    JW_i2(w, 5);
    JW_object(w, &LEVEL_DESC, level);
    JW_i8(w, level->createTime);
    JW_i4(w, level->depth);
    JW_i4(w, level->height);
    JW_f4(w, level->rotSpawn);
    JW_i4(w, level->tickCount);
    JW_i4(w, level->unprocessed);
    JW_i4(w, level->width);
    JW_i4(w, level->xSpawn);
    JW_i4(w, level->ySpawn);
    JW_i4(w, level->zSpawn);
    JW_byteArray(w, level->blocks, level->width * level->height * level->depth);
    writeString(w, level->creator);
    JW_object(w, &LIST_DESC, &level->entities);
    JW_i4(w, level->entityCount + level->playerCount);
    // the array length, not the size
    int cap = level->listCapacity;
    unsigned char c[4] = { (unsigned char)(cap >> 24), (unsigned char)(cap >> 16), (unsigned char)(cap >> 8), (unsigned char)cap };
    JW_block(w, c, 4);
    if (level->playerCount > 0) return false;
    for (int i = 0; i < level->entityCount; ++i) writeZombie(w, &level->entities[i], level);
    JW_endBlock(w);
    writeString(w, level->name);
    return true;
}

void LevelIO_save(const Level* level, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return;
    JWriter w;
    memset(&w, 0, sizeof w);
    bool whole = writeLevel(&w, level);
    size_t n = 0;
    uint8_t* gz = whole ? jgzip_compress(w.buf, w.len, &n) : jgzip_unfinished(w.buf, w.len, &n);
    fwrite(gz, 1, n, f);
    fclose(f);
    free(gz);
    JWriter_free(&w);
}

/* load */

static int32_t be(const uint8_t* p) {
    return (int32_t)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]);
}

static void nodeString(const JNode* n, char* out, size_t size) {
    if (n && n->kind == JN_STRING && n->str) snprintf(out, size, "%s", n->str);
}

static bool loadV2(Level* level, const uint8_t* p, size_t len) {
    JNode* root = NULL;
    JStream* s = JStream_read(p, len, &root);
    if (!s) return false;
    bool ok = JNode_is(root, LV) && root->cls->suid == 0;
    JNode* blocks = ok ? JNode_obj(root, LV, "blocks") : NULL;
    JNode* list = ok ? JNode_obj(root, LV, "entities") : NULL;
    int w = (int)JNode_int(root, LV, "width"), h = (int)JNode_int(root, LV, "height"), d = (int)JNode_int(root, LV, "depth");
    if (!blocks || blocks->kind != JN_ARRAY || !blocks->bytes || w <= 0 || h <= 0 || d <= 0 || blocks->len < w * h * d) ok = false;
    // zombies are the only entities a level can bring back
    if (ok && list) {
        for (int i = 0; i < list->nextra && ok; ++i) {
            const JNode* e = list->extra[i];
            if (e && (!JNode_is(e, ZB) || e->cls->suid != ZOMBIE_DESC.suid)) ok = false;
        }
    }
    if (!ok) {
        JStream_free(s);
        return false;
    }
    byte* data = (byte*)malloc((size_t)w * h * d);
    memcpy(data, blocks->bytes, (size_t)w * h * d);
    int xs = (int)JNode_int(root, LV, "xSpawn"), ys = (int)JNode_int(root, LV, "ySpawn"), zs = (int)JNode_int(root, LV, "zSpawn");
    // initTransient only looks for a spawn when there's none
    Level_setData(level, w, d, h, data);
    if (xs != 0 || ys != 0 || zs != 0) {
        level->xSpawn = xs;
        level->ySpawn = ys;
        level->zSpawn = zs;
    }
    level->rotSpawn = JNode_float(root, LV, "rotSpawn");
    level->createTime = JNode_int(root, LV, "createTime");
    level->tickCount = (int)JNode_int(root, LV, "tickCount");
    level->unprocessed = (int)JNode_int(root, LV, "unprocessed");
    level->name[0] = level->creator[0] = '\0';
    nodeString(JNode_obj(root, LV, "name"), level->name, sizeof level->name);
    nodeString(JNode_obj(root, LV, "creator"), level->creator, sizeof level->creator);
    for (int i = 0; list && i < list->nextra; ++i) {
        const JNode* n = list->extra[i];
        if (!n) continue;
        Zombie zombie;
        Zombie* z = &zombie;
        Zombie_init(z, level, 0.0f, 0.0f, 0.0f);
        Entity* e = &z->base;
        e->boundingBoxHeight = JNode_float(n, EN, "bbHeight");
        e->boundingBoxWidth = JNode_float(n, EN, "bbWidth");
        e->heightOffset = JNode_float(n, EN, "heightOffset");
        e->horizontalCollision = JNode_int(n, EN, "horizontalCollision") != 0;
        e->onGround = JNode_int(n, EN, "onGround") != 0;
        e->removed = JNode_int(n, EN, "removed") != 0;
        e->x = JNode_float(n, EN, "x");
        e->y = JNode_float(n, EN, "y");
        e->z = JNode_float(n, EN, "z");
        e->prevX = JNode_float(n, EN, "xo");
        e->prevY = JNode_float(n, EN, "yo");
        e->prevZ = JNode_float(n, EN, "zo");
        e->motionX = JNode_float(n, EN, "xd");
        e->motionY = JNode_float(n, EN, "yd");
        e->motionZ = JNode_float(n, EN, "zd");
        e->xRotation = JNode_float(n, EN, "xRot");
        e->yRotation = JNode_float(n, EN, "yRot");
        e->xRotO = JNode_float(n, EN, "xRotO");
        e->yRotO = JNode_float(n, EN, "yRotO");
        const JNode* bb = JNode_obj(n, EN, "bb");
        e->boundingBox.minX = JNode_float(bb, NULL, "x0");
        e->boundingBox.minY = JNode_float(bb, NULL, "y0");
        e->boundingBox.minZ = JNode_float(bb, NULL, "z0");
        e->boundingBox.maxX = JNode_float(bb, NULL, "x1");
        e->boundingBox.maxY = JNode_float(bb, NULL, "y1");
        e->boundingBox.maxZ = JNode_float(bb, NULL, "z1");
        z->rotation = JNode_float(n, ZB, "rot");
        z->rotationMotionFactor = JNode_float(n, ZB, "rotA");
        z->speed = JNode_float(n, ZB, "speed");
        z->timeOffset = JNode_float(n, ZB, "timeOffs");
        Level_addEntity(level, z);
    }
    // ArrayList.readObject keeps the saved array length, a missing list is a new one
    level->listCapacity = list && list->hasBlockInt ? list->blockInt : 10;
    JStream_free(s);
    return true;
}

bool LevelIO_load(Level* level, const char* path) {
    Minecraft_beginLevelLoading("Loading level");
    Minecraft_levelLoadUpdate("Reading..");
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    size_t cap = 1 << 20, len = 0;
    uint8_t* file = (uint8_t*)malloc(cap);
    size_t r;
    while ((r = fread(file + len, 1, cap - len, f)) > 0) {
        len += r;
        if (len == cap) file = (uint8_t*)realloc(file, cap *= 2);
    }
    fclose(f);
    char exception[128];
    size_t n = 0;
    uint8_t* data = jgzip_decompress(file, len, &n, exception, sizeof exception);
    free(file);
    if (!data) return false;
    bool ok = false;
    if (n >= 5 && be(data) == MAGIC) {
        signed char version = (signed char)data[4];
        if (version <= 1) ok = Level_load(level, path);
        else if (version == 2) ok = loadV2(level, data + 5, n - 5);
    }
    free(data);
    return ok;
}
