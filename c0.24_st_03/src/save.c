// save.c: level.dat version 1, version 2 can't be written in this version (its BlockMap isn't serializable)

#include <GL/glew.h>
#include "save.h"
#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEVEL_MAGIC 656127880

static int be(const unsigned char* p, int n) {
    int v = 0;
    for (int i = 0; i < n; ++i) v = (v << 8) | p[i];
    return v;
}

bool Save_loadLevel(Level* level, const char* path) {
    gzFile f = gzopen(path, "rb");
    if (!f) return false;
    size_t cap = 1 << 20, len = 0;
    unsigned char* data = (unsigned char*)malloc(cap);
    while (data) {
        if (len == cap) {
            unsigned char* g = (unsigned char*)realloc(data, cap * 2);
            if (!g) { free(data); data = NULL; break; }
            data = g;
            cap *= 2;
        }
        int r = gzread(f, data + len, (unsigned)(cap - len));
        if (r <= 0) break;
        len += (size_t)r;
    }
    gzclose(f);
    if (!data) return false;

    bool ok = false;
    const unsigned char* p = data + 5;
    size_t n = len - 5, pos = 0, s;
    char name[64], creator[64];
    if (len < 5 || be(data, 4) != LEVEL_MAGIC || data[4] > 1) goto done;
    if (pos + 2 > n) goto done;
    s = (size_t)be(p + pos, 2); pos += 2;
    if (pos + s > n) goto done;
    snprintf(name, sizeof name, "%.*s", (int)(s < 63 ? s : 63), (const char*)p + pos); pos += s;
    if (pos + 2 > n) goto done;
    s = (size_t)be(p + pos, 2); pos += 2;
    if (pos + s > n) goto done;
    snprintf(creator, sizeof creator, "%.*s", (int)(s < 63 ? s : 63), (const char*)p + pos); pos += s;
    if (pos + 14 > n) goto done;
    long long createTime = 0;
    for (int i = 0; i < 8; ++i) createTime = (createTime << 8) | p[pos + i];
    pos += 8;
    int w = be(p + pos, 2), h = be(p + pos + 2, 2), d = be(p + pos + 4, 2);
    pos += 6;
    size_t total = (size_t)w * h * d;
    if (total == 0 || pos + total > n) goto done;
    byte* blocks = (byte*)malloc(total);
    if (!blocks) goto done;
    memcpy(blocks, p + pos, total);
    Level_setBlocks(level, w, h, d, blocks);
    memcpy(level->name, name, sizeof name);
    memcpy(level->creator, creator, sizeof creator);
    level->createTime = createTime;
    ok = true;
done:
    free(data);
    return ok;
}

bool Save_loadEntities(World* w) {
    (void)w;
    return false;
}

static void u16(gzFile f, int v) {
    unsigned char b[2] = { (unsigned char)(v >> 8), (unsigned char)v };
    gzwrite(f, b, 2);
}

static void utf(gzFile f, const char* s) {
    size_t n = strlen(s);
    u16(f, (int)n);
    gzwrite(f, s, (unsigned)n);
}

bool Save_write(const World* w, const char* path) {
    const Level* lv = w->level;
    gzFile f = gzopen(path, "wb");
    if (!f) return false;
    unsigned char head[5] = { (unsigned char)(LEVEL_MAGIC >> 24), (unsigned char)(LEVEL_MAGIC >> 16),
                              (unsigned char)(LEVEL_MAGIC >> 8), (unsigned char)LEVEL_MAGIC, 1 };
    gzwrite(f, head, 5);
    utf(f, lv->name);
    utf(f, lv->creator);
    unsigned char t[8];
    for (int i = 0; i < 8; ++i) t[i] = (unsigned char)(lv->createTime >> (56 - i * 8));
    gzwrite(f, t, 8);
    u16(f, lv->width);
    u16(f, lv->height);
    u16(f, lv->depth);
    gzwrite(f, lv->blocks, (unsigned)((size_t)lv->width * lv->height * lv->depth));
    return gzclose(f) == Z_OK;
}
