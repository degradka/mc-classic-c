// level/level_renderer.c

#include <GL/glew.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>

#include "level_renderer.h"
#include "level.h"
#include "chunk.h"
#include "../renderer/frustum.h"
#include "tile/tile.h"
#include "../renderer/textures.h"
#include "../timer.h"
#include "../hitresult.h"
#include "../player.h"
#include "../character/zombie.h"
#include "../java_sort.h"
#include <math.h>
#include <stdio.h>

static Tessellator TESSELLATOR;

static void compileSurroundingGround(LevelRenderer* r) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, r->rockTex);
    float c = 0.5f;
    glColor4f(0.5f, c, c, 1.0f);
    float ground = Level_getGroundLevel(r->level);
    int s = 128;
    if (s > r->level->width)  s = r->level->width;
    if (s > r->level->height) s = r->level->height;
    int d = 2048 / s;

    Tessellator_begin(&TESSELLATOR);
    for (int xx = -s * d; xx < r->level->width + s * d; xx += s) {
        for (int zz = -s * d; zz < r->level->height + s * d; zz += s) {
            float yy = ground;
            if (xx >= 0 && zz >= 0 && xx < r->level->width && zz < r->level->height) yy = 0.0f;
            Tessellator_vertexUV(&TESSELLATOR, (float)(xx + 0), yy, (float)(zz + s), 0.0f, (float)s);
            Tessellator_vertexUV(&TESSELLATOR, (float)(xx + s), yy, (float)(zz + s), (float)s, (float)s);
            Tessellator_vertexUV(&TESSELLATOR, (float)(xx + s), yy, (float)(zz + 0), (float)s, 0.0f);
            Tessellator_vertexUV(&TESSELLATOR, (float)(xx + 0), yy, (float)(zz + 0), 0.0f, 0.0f);
        }
    }
    Tessellator_end(&TESSELLATOR);

    // the tessellator draws at end, so all the walls get the last color, 0.6
    glBindTexture(GL_TEXTURE_2D, r->rockTex);
    glColor3f(0.8f, 0.8f, 0.8f);
    Tessellator_begin(&TESSELLATOR);
    for (int xx = 0; xx < r->level->width; xx += s) {
        Tessellator_vertexUV(&TESSELLATOR, (float)(xx + 0), 0.0f, 0.0f, 0.0f, 0.0f);
        Tessellator_vertexUV(&TESSELLATOR, (float)(xx + s), 0.0f, 0.0f, (float)s, 0.0f);
        Tessellator_vertexUV(&TESSELLATOR, (float)(xx + s), ground, 0.0f, (float)s, ground);
        Tessellator_vertexUV(&TESSELLATOR, (float)(xx + 0), ground, 0.0f, 0.0f, ground);
        Tessellator_vertexUV(&TESSELLATOR, (float)(xx + 0), ground, (float)r->level->height, 0.0f, ground);
        Tessellator_vertexUV(&TESSELLATOR, (float)(xx + s), ground, (float)r->level->height, (float)s, ground);
        Tessellator_vertexUV(&TESSELLATOR, (float)(xx + s), 0.0f, (float)r->level->height, (float)s, 0.0f);
        Tessellator_vertexUV(&TESSELLATOR, (float)(xx + 0), 0.0f, (float)r->level->height, 0.0f, 0.0f);
    }
    glColor3f(0.6f, 0.6f, 0.6f);
    for (int zz = 0; zz < r->level->height; zz += s) {
        Tessellator_vertexUV(&TESSELLATOR, 0.0f, ground, (float)(zz + 0), 0.0f, 0.0f);
        Tessellator_vertexUV(&TESSELLATOR, 0.0f, ground, (float)(zz + s), (float)s, 0.0f);
        Tessellator_vertexUV(&TESSELLATOR, 0.0f, 0.0f, (float)(zz + s), (float)s, ground);
        Tessellator_vertexUV(&TESSELLATOR, 0.0f, 0.0f, (float)(zz + 0), 0.0f, ground);
        Tessellator_vertexUV(&TESSELLATOR, (float)r->level->width, 0.0f, (float)(zz + 0), 0.0f, ground);
        Tessellator_vertexUV(&TESSELLATOR, (float)r->level->width, 0.0f, (float)(zz + s), (float)s, ground);
        Tessellator_vertexUV(&TESSELLATOR, (float)r->level->width, ground, (float)(zz + s), (float)s, 0.0f);
        Tessellator_vertexUV(&TESSELLATOR, (float)r->level->width, ground, (float)(zz + 0), 0.0f, 0.0f);
    }
    Tessellator_end(&TESSELLATOR);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

static void compileSurroundingWater(LevelRenderer* r) {
    glEnable(GL_TEXTURE_2D);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, r->waterTex);
    float water = Level_getWaterLevel(r->level);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    int s = 128;
    if (s > r->level->width)  s = r->level->width;
    if (s > r->level->height) s = r->level->height;
    int d = 2048 / s;

    Tessellator_begin(&TESSELLATOR);
    for (int xx = -s * d; xx < r->level->width + s * d; xx += s) {
        for (int zz = -s * d; zz < r->level->height + s * d; zz += s) {
            float yy = water - 0.1f;
            if (xx >= 0 && zz >= 0 && xx < r->level->width && zz < r->level->height) continue;
            Tessellator_vertexUV(&TESSELLATOR, (float)(xx + 0), yy, (float)(zz + s), 0.0f, (float)s);
            Tessellator_vertexUV(&TESSELLATOR, (float)(xx + s), yy, (float)(zz + s), (float)s, (float)s);
            Tessellator_vertexUV(&TESSELLATOR, (float)(xx + s), yy, (float)(zz + 0), (float)s, 0.0f);
            Tessellator_vertexUV(&TESSELLATOR, (float)(xx + 0), yy, (float)(zz + 0), 0.0f, 0.0f);
            Tessellator_vertexUV(&TESSELLATOR, (float)(xx + 0), yy, (float)(zz + 0), 0.0f, 0.0f);
            Tessellator_vertexUV(&TESSELLATOR, (float)(xx + s), yy, (float)(zz + 0), (float)s, 0.0f);
            Tessellator_vertexUV(&TESSELLATOR, (float)(xx + s), yy, (float)(zz + s), (float)s, (float)s);
            Tessellator_vertexUV(&TESSELLATOR, (float)(xx + 0), yy, (float)(zz + s), 0.0f, (float)s);
        }
    }
    Tessellator_end(&TESSELLATOR);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

void LevelRenderer_init(LevelRenderer* r, int terrainTex) {
    r->level = NULL;
    r->grid = NULL;
    r->chunks = NULL;
    r->callLists = NULL;
    r->xChunks = r->yChunks = r->zChunks = 0;
    r->terrainTex = terrainTex;
    r->rockTex = loadTexture("resources/rock.png", GL_NEAREST);
    r->waterTex = loadTexture("resources/water.png", GL_NEAREST);
    r->cloudsTex = loadTexture("resources/clouds.png", GL_NEAREST);
    r->drawDistance = 0;
    r->cloudTickCounter = 0;
    r->lX = r->lY = r->lZ = -9999.0f;
    r->surroundLists = glGenLists(2);
}

void LevelRenderer_setLevel(LevelRenderer* r, Level* level) {
    if (r->level) r->level->renderer = NULL;
    r->level = level;
    level->renderer = r;
    LevelRenderer_allChanged(r);
}

void LevelRenderer_allChanged(LevelRenderer* r) {
    Level* level = r->level;
    int oldTotal = r->xChunks * r->yChunks * r->zChunks;
    for (int i = 0; i < oldTotal; ++i) Chunk_destroy(&r->grid[i]);
    free(r->grid);
    free(r->chunks);
    free(r->callLists);

    r->xChunks = level->width / 16;
    r->yChunks = level->depth / 16;
    r->zChunks = level->height / 16;
    int total = r->xChunks * r->yChunks * r->zChunks;
    r->grid = (Chunk*)malloc((size_t)total * sizeof(Chunk));
    r->chunks = (Chunk**)malloc((size_t)total * sizeof(Chunk*));
    r->callLists = (unsigned int*)malloc((size_t)total * sizeof(unsigned int));
    if (!r->grid || !r->chunks || !r->callLists) { fprintf(stderr, "Failed to allocate chunks\n"); exit(EXIT_FAILURE); }

    for (int x = 0; x < r->xChunks; x++)
    for (int y = 0; y < r->yChunks; y++)
    for (int z = 0; z < r->zChunks; z++) {
        int idx = (z * r->yChunks + y) * r->xChunks + x;
        Chunk_init(&r->grid[idx], level, x << 4, y << 4, z << 4);
        r->chunks[idx] = &r->grid[idx];
    }

    glNewList(r->surroundLists + 0, GL_COMPILE);
    compileSurroundingGround(r);
    glEndList();
    glNewList(r->surroundLists + 1, GL_COMPILE);
    compileSurroundingWater(r);
    glEndList();

    LevelRenderer_setDirty(r, 0, 0, 0, level->width, level->depth, level->height);
}

void LevelRenderer_cull(LevelRenderer* r, const Frustum* frustum) {
    int total = r->xChunks * r->yChunks * r->zChunks;
    for (int i = 0; i < total; ++i) Chunk_cull(&r->grid[i], frustum);
}

void LevelRenderer_renderSurroundingGround(const LevelRenderer* r) {
    glCallList(r->surroundLists + 0);
}

void LevelRenderer_renderSurroundingWater(const LevelRenderer* r) {
    glCallList(r->surroundLists + 1);
}

void LevelRenderer_renderEntities(LevelRenderer* r, const Frustum* frustum, float a) {
    for (int i = 0; i < r->level->entityCount; ++i) {
        const Zombie* z = &r->level->entities[i];
        if (frustum_isVisible(frustum, &z->base.boundingBox)) Zombie_render(z, a);
    }
}

// DistanceSorter
static int distance_cmp(const void* a, const void* b, void* ctx) {
    const Player* player = (const Player*)ctx;
    return Chunk_distanceToSqr((const Chunk*)a, player) < Chunk_distanceToSqr((const Chunk*)b, player) ? -1 : 1;
}

void LevelRenderer_render(LevelRenderer* r, const Player* player, int layer) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, r->terrainTex);
    int total = r->xChunks * r->yChunks * r->zChunks;

    float xd = player->e.x - r->lX;
    float yd = player->e.y - r->lY;
    float zd = player->e.z - r->lZ;
    if (xd * xd + yd * yd + zd * zd > 64.0f) {
        r->lX = player->e.x;
        r->lY = player->e.y;
        r->lZ = player->e.z;
        JavaSort_sort((void**)r->chunks, total, distance_cmp, (void*)player);
    }

    int count = 0;
    for (int i = 0; i < total; ++i) Chunk_addList(r->chunks[i], r->callLists, &count, layer);
    if (count > 0) glCallLists(count, GL_UNSIGNED_INT, r->callLists);
    glDisable(GL_TEXTURE_2D);
}

void LevelRenderer_renderClouds(LevelRenderer* r, float a) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, r->cloudsTex);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    float scale = 4.8828125E-4f;
    float y = (float)(r->level->depth + 2);
    float off = ((float)r->cloudTickCounter + a) * scale * 0.03f;

    Tessellator_begin(&TESSELLATOR);
    for (int x = -2048; x < r->level->width + 2048; x += 512) {
        for (int z = -2048; z < r->level->height + 2048; z += 512) {
            Tessellator_vertexUV(&TESSELLATOR, (float)x, y, (float)(z + 512), (float)x * scale + off, (float)(z + 512) * scale);
            Tessellator_vertexUV(&TESSELLATOR, (float)(x + 512), y, (float)(z + 512), (float)(x + 512) * scale + off, (float)(z + 512) * scale);
            Tessellator_vertexUV(&TESSELLATOR, (float)(x + 512), y, (float)z, (float)(x + 512) * scale + off, (float)z * scale);
            Tessellator_vertexUV(&TESSELLATOR, (float)x, y, (float)z, (float)x * scale + off, (float)z * scale);
            Tessellator_vertexUV(&TESSELLATOR, (float)x, y, (float)z, (float)x * scale + off, (float)z * scale);
            Tessellator_vertexUV(&TESSELLATOR, (float)(x + 512), y, (float)z, (float)(x + 512) * scale + off, (float)z * scale);
            Tessellator_vertexUV(&TESSELLATOR, (float)(x + 512), y, (float)(z + 512), (float)(x + 512) * scale + off, (float)(z + 512) * scale);
            Tessellator_vertexUV(&TESSELLATOR, (float)x, y, (float)(z + 512), (float)x * scale + off, (float)(z + 512) * scale);
        }
    }
    Tessellator_end(&TESSELLATOR);
    glDisable(GL_TEXTURE_2D);

    // the sky plane above the clouds
    Tessellator_begin(&TESSELLATOR);
    Tessellator_color(&TESSELLATOR, 0.5f, 0.8f, 1.0f);
    y = (float)(r->level->depth + 10);
    for (int x = -2048; x < r->level->width + 2048; x += 512) {
        for (int z = -2048; z < r->level->height + 2048; z += 512) {
            Tessellator_vertex(&TESSELLATOR, (float)x, y, (float)z);
            Tessellator_vertex(&TESSELLATOR, (float)(x + 512), y, (float)z);
            Tessellator_vertex(&TESSELLATOR, (float)(x + 512), y, (float)(z + 512));
            Tessellator_vertex(&TESSELLATOR, (float)x, y, (float)(z + 512));
        }
    }
    Tessellator_end(&TESSELLATOR);
}

void LevelRenderer_renderInside(LevelRenderer* r, int x, int y, int z) {
    int id = Level_getTile(r->level, x, y, z);
    if (id == 0 || !gTiles[id]->isSolid(gTiles[id])) return;
    glEnable(GL_TEXTURE_2D);
    glColor4f(0.2f, 0.2f, 0.2f, 1.0f);
    glDepthFunc(GL_LESS);
    Tessellator_begin(&TESSELLATOR);
    for (int i = 0; i < 6; ++i) gTiles[id]->renderFace(gTiles[id], &TESSELLATOR, x, y, z, i);
    Tessellator_end(&TESSELLATOR);
    glCullFace(GL_FRONT);
    Tessellator_begin(&TESSELLATOR);
    for (int i = 0; i < 6; ++i) gTiles[id]->renderFace(gTiles[id], &TESSELLATOR, x, y, z, i);
    Tessellator_end(&TESSELLATOR);
    glCullFace(GL_BACK);
    glDisable(GL_TEXTURE_2D);
    glDepthFunc(GL_LEQUAL);
}

void LevelRenderer_destroy(LevelRenderer* r) {
    int total = r->xChunks * r->yChunks * r->zChunks;
    for (int i = 0; i < total; ++i) Chunk_destroy(&r->grid[i]);
    glDeleteLists(r->surroundLists, 2);
    free(r->grid);
    free(r->chunks);
    free(r->callLists);
}

// DirtyChunkSorter
static int dirty_cmp(const void* a, const void* b, void* ctx) {
    const Player* player = (const Player*)ctx;
    const Chunk* c0 = (const Chunk*)a;
    const Chunk* c1 = (const Chunk*)b;
    if (c0->visible && !c1->visible) return -1;
    if (c1->visible && !c0->visible) return 1;
    return Chunk_distanceToSqr(c0, player) < Chunk_distanceToSqr(c1, player) ? -1 : 1;
}

// the dirty set goes through a TreeSet, ties keep the identity hash order
void LevelRenderer_updateDirtyChunks(LevelRenderer* r, const Player* player) {
    int total = r->xChunks * r->yChunks * r->zChunks;
    Chunk** dirty = (Chunk**)malloc((size_t)(total > 0 ? total : 1) * sizeof(Chunk*));
    if (!dirty) return;
    int n = 0;
    for (int i = 0; i < total; ++i) {
        if (r->grid[i].dirty) dirty[n++] = &r->grid[i];
    }
    if (n > 0) {
        JavaSort_sort((void**)dirty, n, dirty_cmp, (void*)player);
        for (int i = 0; i < 4 && i < n; ++i) {
            Chunk_rebuild(dirty[i]);
            dirty[i]->dirty = false;
        }
    }
    free(dirty);
}

void LevelRenderer_renderHit(LevelRenderer* r, const Player* player, const HitResult* h, int mode, int tileType) {
    glEnable(GL_BLEND);
    glEnable(GL_ALPHA_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(1.0f, 1.0f, 1.0f, ((float)sin((double)currentTimeMillis() / 100.0) * 0.2f + 0.4f) * 0.5f);
    if (mode == 0) {
        Tessellator_begin(&TESSELLATOR);
        for (int i = 0; i < 6; i++) {
            Face_render(&TESSELLATOR, h->x, h->y, h->z, i, player->e.x, player->e.y, player->e.z);
        }
        Tessellator_end(&TESSELLATOR);
    } else {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        float br = (float)sin((double)currentTimeMillis() / 100.0) * 0.2f + 0.8f;
        glColor4f(br, br, br, (float)sin((double)currentTimeMillis() / 200.0) * 0.2f + 0.5f);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, r->terrainTex);
        int x = h->x;
        int y = h->y;
        int z = h->z;
        if (h->f == 0) y--;
        if (h->f == 1) y++;
        if (h->f == 2) z--;
        if (h->f == 3) z++;
        if (h->f == 4) x--;
        if (h->f == 5) x++;
        Tessellator_begin(&TESSELLATOR);
        Tessellator_setIgnoreColor(&TESSELLATOR, 1);
        gTiles[tileType]->render(gTiles[tileType], &TESSELLATOR, r->level, 0, x, y, z);
        gTiles[tileType]->render(gTiles[tileType], &TESSELLATOR, r->level, 1, x, y, z);
        Tessellator_end(&TESSELLATOR);
        glDisable(GL_TEXTURE_2D);
    }
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
}

void LevelRenderer_renderHitOutline(const HitResult* h, int mode) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.4f);

    float x = (float)h->x, y = (float)h->y, z = (float)h->z;
    if (mode == 1) {
        if (h->f == 0) y--;
        if (h->f == 1) y++;
        if (h->f == 2) z--;
        if (h->f == 3) z++;
        if (h->f == 4) x--;
        if (h->f == 5) x++;
    }

    glBegin(GL_LINE_STRIP);
    glVertex3f(x,        y, z);
    glVertex3f(x + 1.0f, y, z);
    glVertex3f(x + 1.0f, y, z + 1.0f);
    glVertex3f(x,        y, z + 1.0f);
    glVertex3f(x,        y, z);
    glEnd();

    glBegin(GL_LINE_STRIP);
    glVertex3f(x,        y + 1.0f, z);
    glVertex3f(x + 1.0f, y + 1.0f, z);
    glVertex3f(x + 1.0f, y + 1.0f, z + 1.0f);
    glVertex3f(x,        y + 1.0f, z + 1.0f);
    glVertex3f(x,        y + 1.0f, z);
    glEnd();

    glBegin(GL_LINES);
    glVertex3f(x,        y,        z);        glVertex3f(x,        y + 1.0f, z);
    glVertex3f(x + 1.0f, y,        z);        glVertex3f(x + 1.0f, y + 1.0f, z);
    glVertex3f(x + 1.0f, y,        z + 1.0f); glVertex3f(x + 1.0f, y + 1.0f, z + 1.0f);
    glVertex3f(x,        y,        z + 1.0f); glVertex3f(x,        y + 1.0f, z + 1.0f);
    glEnd();

    glDisable(GL_BLEND);
}

void LevelRenderer_setDirty(LevelRenderer* r, int x0, int y0, int z0, int x1, int y1, int z1) {
    x0 /= 16; y0 /= 16; z0 /= 16;
    x1 /= 16; y1 /= 16; z1 /= 16;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (z0 < 0) z0 = 0;
    if (x1 > r->xChunks - 1) x1 = r->xChunks - 1;
    if (y1 > r->yChunks - 1) y1 = r->yChunks - 1;
    if (z1 > r->zChunks - 1) z1 = r->zChunks - 1;
    for (int x = x0; x <= x1; ++x)
    for (int y = y0; y <= y1; ++y)
    for (int z = z0; z <= z1; ++z) {
        r->grid[(z * r->yChunks + y) * r->xChunks + x].dirty = true;
    }
}

void levelRenderer_tileChanged(LevelRenderer* r, int x, int y, int z) {
    LevelRenderer_setDirty(r, x - 1, y - 1, z - 1, x + 1, y + 1, z + 1);
}

void levelRenderer_lightColumnChanged(LevelRenderer* r, int x, int z, int minY, int maxY) {
    LevelRenderer_setDirty(r, x - 1, minY - 1, z - 1, x + 1, maxY + 1, z + 1);
}
