// level/levelrenderer.c — chunk grid, frustum culling, dirty-marking, and hit highlight

#include <GL/glew.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>

#include "levelrenderer.h"
#include "level.h"
#include "chunk.h"
#include "../renderer/frustum.h"
#include "tile/tile.h"
#include "../timer.h"
#include "../hitresult.h"
#include "../player.h"
#include <math.h>
#include <stdio.h>

extern Tessellator TESSELLATOR;  // use the global, like chunks do

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
    frustum_calculate();

    int total = r->chunkAmountX * r->chunkAmountY * r->chunkAmountZ;
    for (int i = 0; i < total; ++i) {
        if (frustum_isVisible(&r->chunks[i].boundingBox)) {
            Chunk_render(&r->chunks[i], layer);
        }
    }
}

void LevelRenderer_destroy(LevelRenderer* r) {
    free(r->chunks);
}

/* --- dirty-chunk prioritization -------------------------------------------- */

// sort state for qsort comparator
static const Player* gSortPlayer = NULL;
static long long gSortNow = 0;

static int dirty_cmp(const void* a, const void* b) {
    const Chunk* c1 = *(const Chunk* const*)a;
    const Chunk* c2 = *(const Chunk* const*)b;

    if (c1 == c2) return 0;

    // visible chunks first
    int v1 = frustum_isVisible(&c1->boundingBox);
    int v2 = frustum_isVisible(&c2->boundingBox);
    if (v1 && !v2) return -1;
    if (v2 && !v1) return  1;

    // higher priority to larger dirty duration (mirror Java logic: they compare ints after /2000)
    int d1 = (int)((gSortNow - c1->dirtiedTime) / 2000LL);
    int d2 = (int)((gSortNow - c2->dirtiedTime) / 2000LL);
    if (d1 < d2) return -1;
    if (d1 > d2) return  1;

    // finally, closer to player first
    double dist1 = Chunk_distanceToSqr(c1, gSortPlayer);
    double dist2 = Chunk_distanceToSqr(c2, gSortPlayer);
    return (dist1 < dist2) ? -1 : 1;
}

int LevelRenderer_updateDirtyChunks(LevelRenderer* r, const Player* player) {
    frustum_calculate(); // ensure frustum up-to-date for visibility test

    // collect dirty chunk pointers
    int total = r->chunkAmountX * r->chunkAmountY * r->chunkAmountZ;
    Chunk** list = (Chunk**)malloc((size_t)total * sizeof(Chunk*));
    if (!list) return 0;

    int n = 0;
    for (int i = 0; i < total; ++i) {
        if (Chunk_isDirty(&r->chunks[i])) list[n++] = &r->chunks[i];
    }
    if (n == 0) { free(list); return 0; }

    // sort with priorities
    gSortPlayer = player;
    gSortNow    = currentTimeMillis();
    qsort(list, (size_t)n, sizeof(Chunk*), dirty_cmp);

    // rebuild up to 8 per frame
    int limit = n < 8 ? n : 8;
    for (int i = 0; i < limit; ++i) {
        Chunk_rebuild(list[i], 0);
        Chunk_rebuild(list[i], 1);
    }

    free(list);
    return limit;
}

/*
    - mode 0: additive pulsing, untextured full-cube outline (all 6 faces)
    - mode 1: alpha-blended, textured preview block on the adjacent cell,
              rendered in both layers (0 & 1)
*/
void LevelRenderer_renderHit(LevelRenderer* r, HitResult* h, int mode, int tileId) {
    if (!h) return;

    if (mode == 0) {
        // --- Destroy highlight: additive, pulsing; draw all 6 faces untextured
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        float a = (float)(sin((double)currentTimeMillis() / 100.0) * 0.2 + 0.4) * 0.5f;
        glColor4f(1.f, 1.f, 1.f, a);

        Tessellator_init(&TESSELLATOR);
        for (int face = 0; face < 6; ++face) {
            Face_render(&TESSELLATOR, h->x, h->y, h->z, face);
        }
        Tessellator_flush(&TESSELLATOR);

        glDisable(GL_BLEND);
        return;
    }

    // --- Place preview: alpha blend, pulsing tint+alpha on adjacent cell
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float br = (float)(sin((double)currentTimeMillis() / 100.0) * 0.2 + 0.8);
    float al = (float)(sin((double)currentTimeMillis() / 200.0) * 0.2 + 0.5);
    glColor4f(br, br, br, al);

    int nx = 0, ny = 0, nz = 0;
    switch (h->f) {
        case 0: ny = -1; break; // bottom
        case 1: ny =  1; break; // top
        case 2: nz = -1; break; // -Z
        case 3: nz =  1; break; // +Z
        case 4: nx = -1; break; // -X
        case 5: nx =  1; break; // +X
    }
    const int x = h->x + nx, y = h->y + ny, z = h->z + nz;

    const Tile* t = (tileId >= 0 && tileId < 256) ? gTiles[tileId] : NULL;
    if (t && t->render) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, r->terrainTex);

        Tessellator_setIgnoreColor(&TESSELLATOR, 1); // Java: t.noColor()
        Tessellator_init(&TESSELLATOR);
        t->render(t, &TESSELLATOR, r->level, 0, x, y, z);
        t->render(t, &TESSELLATOR, r->level, 1, x, y, z);
        Tessellator_flush(&TESSELLATOR);
        Tessellator_setIgnoreColor(&TESSELLATOR, 0);

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
