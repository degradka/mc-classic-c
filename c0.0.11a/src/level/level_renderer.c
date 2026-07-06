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
#include "../timer.h"
#include "../hitresult.h"
#include "../player.h"
#include "../java_sort.h"
#include <math.h>
#include <stdio.h>

static Tessellator TESSELLATOR;

extern Tessellator TESSELLATOR;
Frustum frustum;

void LevelRenderer_init(LevelRenderer* r, Level* level, int terrainTex) {
    r->chunkAmountX = level->width  / CHUNK_SIZE;
    r->chunkAmountY = level->depth  / CHUNK_SIZE;
    r->chunkAmountZ = level->height / CHUNK_SIZE;

    r->level        = level;
    r->terrainTex   = terrainTex;
    level->renderer = r;

    int total = r->chunkAmountX * r->chunkAmountY * r->chunkAmountZ;
    r->chunks = (Chunk*)malloc((size_t)total * sizeof(Chunk));
    if (!r->chunks) { fprintf(stderr, "Failed to allocate chunks\n"); exit(EXIT_FAILURE); }

    for (int x = 0; x < r->chunkAmountX; x++)
    for (int y = 0; y < r->chunkAmountY; y++)
    for (int z = 0; z < r->chunkAmountZ; z++) {
        int minChunkX = x * CHUNK_SIZE, maxChunkX = MIN(level->width,  (x + 1) * CHUNK_SIZE);
        int minChunkY = y * CHUNK_SIZE, maxChunkY = MIN(level->depth,  (y + 1) * CHUNK_SIZE);
        int minChunkZ = z * CHUNK_SIZE, maxChunkZ = MIN(level->height, (z + 1) * CHUNK_SIZE);
        Chunk_init(&r->chunks[(x + y * r->chunkAmountX) * r->chunkAmountZ + z],
                   level, minChunkX, minChunkY, minChunkZ, maxChunkX, maxChunkY, maxChunkZ);
    }
}

void LevelRenderer_render(const LevelRenderer* r, int layer) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, r->terrainTex);
    frustum_calculate(&frustum);

    int total = r->chunkAmountX * r->chunkAmountY * r->chunkAmountZ;
    for (int i = 0; i < total; ++i) {
        if (frustum_isVisible(&frustum, &r->chunks[i].boundingBox)) {
            Chunk_render(&r->chunks[i], layer);
        }
    }
    glDisable(GL_TEXTURE_2D);
}

void LevelRenderer_destroy(LevelRenderer* r) {
    free(r->chunks);
}

/* dirty chunk prioritization */

static const Player* gSortPlayer = NULL;
static long long gSortNow = 0;

// DirtyChunkSorter, never returns 0
static int dirty_cmp(const void* a, const void* b, void* ctx) {
    (void)ctx;
    const Chunk* c1 = (const Chunk*)a;
    const Chunk* c2 = (const Chunk*)b;

    // visible chunks first
    int v1 = frustum_isVisible(&frustum, &c1->boundingBox);
    int v2 = frustum_isVisible(&frustum, &c2->boundingBox);
    if (v1 && !v2) return -1;
    if (v2 && !v1) return  1;

    // compared in steps of 2000 ms, the most recently dirtied first
    int d1 = (int)((gSortNow - c1->dirtiedTime) / 2000LL);
    int d2 = (int)((gSortNow - c2->dirtiedTime) / 2000LL);
    if (d1 < d2) return -1;
    if (d1 > d2) return  1;

    // finally, closer to player first
    return Chunk_distanceToSqr(c1, gSortPlayer) < Chunk_distanceToSqr(c2, gSortPlayer) ? -1 : 1;
}

void LevelRenderer_updateDirtyChunks(LevelRenderer* r, const Player* player) {
    frustum_calculate(&frustum);

    // collect dirty chunk pointers
    int total = r->chunkAmountX * r->chunkAmountY * r->chunkAmountZ;
    Chunk** list = (Chunk**)malloc((size_t)total * sizeof(Chunk*));
    if (!list) return;

    int n = 0;
    for (int i = 0; i < total; ++i) {
        if (Chunk_isDirty(&r->chunks[i])) list[n++] = &r->chunks[i];
    }
    if (n == 0) { free(list); return; }

    // sort with priorities
    gSortPlayer = player;
    gSortNow    = currentTimeMillis();
    JavaSort_sort((void**)list, n, dirty_cmp, NULL);

    // rebuild up to 8 per frame
    int limit = n < 8 ? n : 8;
    for (int i = 0; i < limit; ++i) {
        Chunk_rebuild(list[i], 0);
        Chunk_rebuild(list[i], 1);
    }

    free(list);
}

void LevelRenderer_pick(LevelRenderer* r, const Player* player, const Frustum* f) {
    float rr = 3.0f;
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
                if (tile && frustum_isVisible(f, &tileBox)) {
                    glLoadName(z);
                    glPushName(0);
                    for (int i = 0; i < 6; i++) {
                        glLoadName(i);
                        Tessellator_init(&TESSELLATOR);
                        Face_render(&TESSELLATOR, x, y, z, i);
                        Tessellator_flush(&TESSELLATOR);
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

// mode 0 pulses additively on all 6 faces
// mode 1 is a see through preview block on the next cell
void LevelRenderer_renderHit(LevelRenderer* r, HitResult* h, int mode, int tileId) {
    if (!h) return;

    if (mode == 0) {
        // destroy highlight: additive, pulsing, draws all 6 faces untextured
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        float a = ((float)sin((double)currentTimeMillis() / 100.0) * 0.2f + 0.4f) * 0.5f;
        glColor4f(1.f, 1.f, 1.f, a);

        Tessellator_init(&TESSELLATOR);
        for (int face = 0; face < 6; ++face) {
            Face_render(&TESSELLATOR, h->x, h->y, h->z, face);
        }
        Tessellator_flush(&TESSELLATOR);

        glDisable(GL_BLEND);
        return;
    }

    // place preview: alpha blend, pulsing tint and alpha on adjacent cell
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float br = (float)sin((double)currentTimeMillis() / 100.0) * 0.2f + 0.8f;
    float al = (float)sin((double)currentTimeMillis() / 200.0) * 0.2f + 0.5f;
    glColor4f(br, br, br, al);

    int nx = 0, ny = 0, nz = 0;
    switch (h->f) {
        case 0: ny = -1; break; // bottom
        case 1: ny =  1; break; // top
        case 2: nz = -1; break; // negative Z
        case 3: nz =  1; break; // positive Z
        case 4: nx = -1; break; // negative X
        case 5: nx =  1; break; // positive X
    }
    const int x = h->x + nx, y = h->y + ny, z = h->z + nz;

    const Tile* t = (tileId >= 0 && tileId < 256) ? gTiles[tileId] : NULL;
    if (t && t->render) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, r->terrainTex);

        Tessellator_init(&TESSELLATOR);
        Tessellator_setIgnoreColor(&TESSELLATOR, 1);
        t->render(t, &TESSELLATOR, r->level, 0, x, y, z);
        t->render(t, &TESSELLATOR, r->level, 1, x, y, z);
        Tessellator_flush(&TESSELLATOR);

        glDisable(GL_TEXTURE_2D);
    }

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

void levelRenderer_allChanged(Level* level, LevelRenderer* r) {
    LevelRenderer_setDirty(r, 0, 0, 0, level->width, level->depth, level->height);
}
