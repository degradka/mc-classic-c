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
#include "../java_sort.h"
#include <math.h>
#include <stdio.h>

static Tessellator TESSELLATOR;

static void compileSurroundingGround(LevelRenderer* r) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, r->rockTex);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    float y = 32.0f - 2.0f;
    int s = 128;
    if (s > r->level->width)  s = r->level->width;
    if (s > r->level->height) s = r->level->height;
    int d = 2048 / s;

    Tessellator_begin(&TESSELLATOR);
    for (int xx = -s * d; xx < r->level->width + s * d; xx += s) {
        for (int zz = -s * d; zz < r->level->height + s * d; zz += s) {
            float yy = y;
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
        Tessellator_vertexUV(&TESSELLATOR, (float)(xx + s), y,    0.0f, (float)s, y);
        Tessellator_vertexUV(&TESSELLATOR, (float)(xx + 0), y,    0.0f, 0.0f, y);
        Tessellator_vertexUV(&TESSELLATOR, (float)(xx + 0), y,    (float)r->level->height, 0.0f, y);
        Tessellator_vertexUV(&TESSELLATOR, (float)(xx + s), y,    (float)r->level->height, (float)s, y);
        Tessellator_vertexUV(&TESSELLATOR, (float)(xx + s), 0.0f, (float)r->level->height, (float)s, 0.0f);
        Tessellator_vertexUV(&TESSELLATOR, (float)(xx + 0), 0.0f, (float)r->level->height, 0.0f, 0.0f);
    }
    glColor3f(0.6f, 0.6f, 0.6f);
    for (int zz = 0; zz < r->level->height; zz += s) {
        Tessellator_vertexUV(&TESSELLATOR, 0.0f, y,    (float)(zz + 0), 0.0f, 0.0f);
        Tessellator_vertexUV(&TESSELLATOR, 0.0f, y,    (float)(zz + s), (float)s, 0.0f);
        Tessellator_vertexUV(&TESSELLATOR, 0.0f, 0.0f, (float)(zz + s), (float)s, y);
        Tessellator_vertexUV(&TESSELLATOR, 0.0f, 0.0f, (float)(zz + 0), 0.0f, y);
        Tessellator_vertexUV(&TESSELLATOR, (float)r->level->width, 0.0f, (float)(zz + 0), 0.0f, y);
        Tessellator_vertexUV(&TESSELLATOR, (float)r->level->width, 0.0f, (float)(zz + s), (float)s, y);
        Tessellator_vertexUV(&TESSELLATOR, (float)r->level->width, y,    (float)(zz + s), (float)s, 0.0f);
        Tessellator_vertexUV(&TESSELLATOR, (float)r->level->width, y,    (float)(zz + 0), 0.0f, 0.0f);
    }
    Tessellator_end(&TESSELLATOR);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

static void compileSurroundingWater(LevelRenderer* r) {
    glEnable(GL_TEXTURE_2D);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, r->waterTex);
    float y = 32.0f;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    int s = 128;
    if (s > r->level->width)  s = r->level->width;
    if (s > r->level->height) s = r->level->height;
    int d = 2048 / s;

    Tessellator_begin(&TESSELLATOR);
    for (int xx = -s * d; xx < r->level->width + s * d; xx += s) {
        for (int zz = -s * d; zz < r->level->height + s * d; zz += s) {
            float yy = y - 0.1f;
            if (xx < 0 || zz < 0 || xx >= r->level->width || zz >= r->level->height) {
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
    }
    Tessellator_end(&TESSELLATOR);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

void LevelRenderer_init(LevelRenderer* r, Level* level, int terrainTex) {
    r->level = level;
    r->terrainTex = terrainTex;
    r->rockTex = loadTexture("resources/rock.png", GL_NEAREST);
    r->waterTex = loadTexture("resources/water.png", GL_NEAREST);
    r->chunks = NULL;
    r->sortedChunks = NULL;
    r->chunkAmountX = r->chunkAmountY = r->chunkAmountZ = 0;
    r->drawDistance = 0;
    r->lX = r->lY = r->lZ = 0.0f;
    level->renderer = r;
    r->surroundLists = glGenLists(2);
    LevelRenderer_allChanged(r);
}

void LevelRenderer_allChanged(LevelRenderer* r) {
    Level* level = r->level;
    int oldTotal = r->chunkAmountX * r->chunkAmountY * r->chunkAmountZ;
    for (int i = 0; i < oldTotal; ++i) Chunk_destroy(&r->chunks[i]);
    free(r->chunks);
    free(r->sortedChunks);

    r->lX = -900000.0f;
    r->lY = -900000.0f;
    r->lZ = -900000.0f;
    r->chunkAmountX = (level->width  + CHUNK_SIZE - 1) / CHUNK_SIZE;
    r->chunkAmountY = (level->depth  + CHUNK_SIZE - 1) / CHUNK_SIZE;
    r->chunkAmountZ = (level->height + CHUNK_SIZE - 1) / CHUNK_SIZE;

    int total = r->chunkAmountX * r->chunkAmountY * r->chunkAmountZ;
    r->chunks = (Chunk*)malloc((size_t)total * sizeof(Chunk));
    r->sortedChunks = (Chunk**)malloc((size_t)total * sizeof(Chunk*));
    if (!r->chunks || !r->sortedChunks) { fprintf(stderr, "Failed to allocate chunks\n"); exit(EXIT_FAILURE); }

    for (int x = 0; x < r->chunkAmountX; x++)
    for (int y = 0; y < r->chunkAmountY; y++)
    for (int z = 0; z < r->chunkAmountZ; z++) {
        int x0 = x * CHUNK_SIZE, y0 = y * CHUNK_SIZE, z0 = z * CHUNK_SIZE;
        int x1 = (x + 1) * CHUNK_SIZE, y1 = (y + 1) * CHUNK_SIZE, z1 = (z + 1) * CHUNK_SIZE;
        if (x1 > level->width)  x1 = level->width;
        if (y1 > level->depth)  y1 = level->depth;
        if (z1 > level->height) z1 = level->height;
        int idx = (x + y * r->chunkAmountX) * r->chunkAmountZ + z;
        Chunk_init(&r->chunks[idx], level, x0, y0, z0, x1, y1, z1);
        r->sortedChunks[idx] = &r->chunks[idx];
    }

    glNewList(r->surroundLists + 0, GL_COMPILE);
    compileSurroundingGround(r);
    glEndList();
    glNewList(r->surroundLists + 1, GL_COMPILE);
    compileSurroundingWater(r);
    glEndList();

    for (int i = 0; i < total; i++) Chunk_reset(&r->chunks[i]);
}

void LevelRenderer_cull(LevelRenderer* r, const Frustum* frustum) {
    int total = r->chunkAmountX * r->chunkAmountY * r->chunkAmountZ;
    for (int i = 0; i < total; ++i) {
        r->chunks[i].visible = frustum_isVisible(frustum, &r->chunks[i].boundingBox) ? true : false;
    }
}

void LevelRenderer_toggleDrawDistance(LevelRenderer* r) {
    r->drawDistance = (r->drawDistance + 1) % 4;
}

void LevelRenderer_renderSurroundingGround(const LevelRenderer* r) {
    glCallList(r->surroundLists + 0);
}

void LevelRenderer_renderSurroundingWater(const LevelRenderer* r) {
    glCallList(r->surroundLists + 1);
}

// DistanceSorter
static int distance_cmp(const void* a, const void* b, void* ctx) {
    const Player* player = (const Player*)ctx;
    return Chunk_distanceToSqr((const Chunk*)a, player) < Chunk_distanceToSqr((const Chunk*)b, player) ? -1 : 1;
}

void LevelRenderer_render(LevelRenderer* r, const Player* player, int layer) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, r->terrainTex);
    int total = r->chunkAmountX * r->chunkAmountY * r->chunkAmountZ;

    float xd = player->e.x - r->lX;
    float yd = player->e.y - r->lY;
    float zd = player->e.z - r->lZ;
    if (xd * xd + yd * yd + zd * zd > 64.0f) {
        r->lX = player->e.x;
        r->lY = player->e.y;
        r->lZ = player->e.z;
        JavaSort_sort((void**)r->sortedChunks, total, distance_cmp, (void*)player);
    }

    for (int i = 0; i < total; ++i) {
        Chunk* c = r->sortedChunks[i];
        if (!c->visible || c->empty) continue;
        float dd = (float)(256 / (1 << r->drawDistance));
        if (r->drawDistance == 0 || Chunk_distanceToSqr(c, player) < dd * dd) Chunk_render(c, layer);
    }
    glDisable(GL_TEXTURE_2D);
}

void LevelRenderer_destroy(LevelRenderer* r) {
    int total = r->chunkAmountX * r->chunkAmountY * r->chunkAmountZ;
    for (int i = 0; i < total; ++i) Chunk_destroy(&r->chunks[i]);
    glDeleteLists(r->surroundLists, 2);
    free(r->chunks);
    free(r->sortedChunks);
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

void LevelRenderer_updateDirtyChunks(LevelRenderer* r, const Player* player) {
    int total = r->chunkAmountX * r->chunkAmountY * r->chunkAmountZ;
    Chunk** dirty = (Chunk**)malloc((size_t)total * sizeof(Chunk*));
    if (!dirty) return;

    int n = 0;
    for (int i = 0; i < total; ++i) {
        if (Chunk_isDirty(&r->chunks[i])) dirty[n++] = &r->chunks[i];
    }
    if (n > 0) {
        JavaSort_sort((void**)dirty, n, dirty_cmp, (void*)player);
        for (int i = 0; i < 4 && i < n; ++i) Chunk_rebuild(dirty[i]);
    }
    free(dirty);
}

void LevelRenderer_pick(LevelRenderer* r, const Player* player, const Frustum* frustum) {
    float rr = 2.5f;
    AABB box = AABB_grow(&player->e.boundingBox, rr, rr, rr);
    int x0 = (int)box.minX;
    int x1 = (int)(box.maxX + 1.0f);
    int y0 = (int)box.minY;
    int y1 = (int)(box.maxY + 1.0f);
    int z0 = (int)box.minZ;
    int z1 = (int)(box.maxZ + 1.0f);

    glInitNames();
    glPushName(0);
    glPushName(0);
    for (int x = x0; x < x1; x++) {
        glLoadName(x);
        glPushName(0);
        for (int y = y0; y < y1; y++) {
            glLoadName(y);
            glPushName(0);
            for (int z = z0; z < z1; z++) {
                const Tile* tile = gTiles[Level_getTile(r->level, x, y, z)];
                AABB tileBox = AABB_create((float)x, (float)y, (float)z, (float)(x + 1), (float)(y + 1), (float)(z + 1));
                if (tile && tile->mayPick(tile) && frustum_isVisible(frustum, &tileBox)) {
                    glLoadName(z);
                    glPushName(0);
                    for (int i = 0; i < 6; i++) {
                        glLoadName(i);
                        Tessellator_begin(&TESSELLATOR);
                        Face_render(&TESSELLATOR, x, y, z, i, player->e.x, player->e.y, player->e.z);
                        Tessellator_end(&TESSELLATOR);
                    }
                    glPopName();
                }
            }
            glPopName();
        }
        glPopName();
    }
    glPopName();
    glPopName();
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

void LevelRenderer_setDirty(const LevelRenderer* r, int minX, int minY, int minZ, int maxX, int maxY, int maxZ) {
    minX /= CHUNK_SIZE; minY /= CHUNK_SIZE; minZ /= CHUNK_SIZE;
    maxX /= CHUNK_SIZE; maxY /= CHUNK_SIZE; maxZ /= CHUNK_SIZE;

    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (minZ < 0) minZ = 0;

    if (maxX >= r->chunkAmountX) maxX = r->chunkAmountX - 1;
    if (maxY >= r->chunkAmountY) maxY = r->chunkAmountY - 1;
    if (maxZ >= r->chunkAmountZ) maxZ = r->chunkAmountZ - 1;

    for (int x = minX; x <= maxX; ++x)
    for (int y = minY; y <= maxY; ++y)
    for (int z = minZ; z <= maxZ; ++z) {
        Chunk_setDirty(&r->chunks[(x + y * r->chunkAmountX) * r->chunkAmountZ + z]);
    }
}

void levelRenderer_tileChanged(LevelRenderer* r, int x, int y, int z) {
    LevelRenderer_setDirty(r, x - 1, y - 1, z - 1, x + 1, y + 1, z + 1);
}

void levelRenderer_lightColumnChanged(LevelRenderer* r, int x, int z, int minY, int maxY) {
    LevelRenderer_setDirty(r, x - 1, minY - 1, z - 1, x + 1, maxY + 1, z + 1);
}
