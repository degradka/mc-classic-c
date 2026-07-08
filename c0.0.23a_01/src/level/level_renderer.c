// level_renderer.c: chunk grid, frustum culling, dirty marking, and hit highlight

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
#include "../renderer/tessellator.h"
#include <math.h>
#include <stdio.h>

static Tessellator TESSELLATOR;

extern Tessellator TESSELLATOR;  // use the global, like chunks do

// c0.0.13a addition: an infinite horizon illusion that tiles rock.png and
// water.png far out past the map edges so the world doesn't look like it
// has a hard boundary. Compiled once into display lists, surroundLists
// slot 0 is ground and slot 1 is water.
static void compileSurroundingGround(LevelRenderer* r) {
    glEnable(GL_TEXTURE_2D);
    // c0.0.19a_04: TextureManager's shared upload path dropped mipmap
    // generation entirely (gluBuild2DMipmaps -> plain glTexImage2D), which
    // applies to every texture it loads, not just the newly animated ones
    GLuint rockTex = loadTexture("resources/rock.png", GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, rockTex);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    const float y = Level_getGroundLevel(r->level);
    int s = 128;
    if (s > r->level->width)  s = r->level->width;
    if (s > r->level->height) s = r->level->height;
    // c0.0.13a_03 keeps the skirt span at a fixed ~2048 units regardless of
    // tile size, instead of the old 5x proportional to the clamped tile size
    const int d = 2048 / s;

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

    // walls at the map edge down to the ground plane
    glBindTexture(GL_TEXTURE_2D, rockTex);
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
    // c0.0.19a_04: see compileSurroundingGround, same mipmap removal applies
    GLuint waterTex = loadTexture("resources/water.png", GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, waterTex);

    const float y = Level_getWaterLevel(r->level);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int s = 128;
    if (s > r->level->width)  s = r->level->width;
    if (s > r->level->height) s = r->level->height;
    // c0.0.13a_03 keeps the skirt span at a fixed ~2048 units regardless of
    // tile size, instead of the old 5x proportional to the clamped tile size
    const int d = 2048 / s;

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
    r->chunkAmountX = level->width  / CHUNK_SIZE;
    r->chunkAmountY = level->depth  / CHUNK_SIZE;
    r->chunkAmountZ = level->height / CHUNK_SIZE;

    r->level        = level;
    r->terrainTex   = terrainTex;
    level->renderer = r;

    int total = r->chunkAmountX * r->chunkAmountY * r->chunkAmountZ;
    r->chunks = (Chunk*)malloc((size_t)total * sizeof(Chunk));
    r->sortedChunks = (Chunk**)malloc((size_t)total * sizeof(Chunk*));
    r->dirtyScratch = (Chunk**)malloc((size_t)total * sizeof(Chunk*));
    if (!r->chunks || !r->sortedChunks || !r->dirtyScratch) { fprintf(stderr, "Failed to allocate chunks\n"); exit(EXIT_FAILURE); }

    for (int x = 0; x < r->chunkAmountX; x++)
    for (int y = 0; y < r->chunkAmountY; y++)
    for (int z = 0; z < r->chunkAmountZ; z++) {
        int minChunkX = x * CHUNK_SIZE, maxChunkX = MIN(level->width,  (x + 1) * CHUNK_SIZE);
        int minChunkY = y * CHUNK_SIZE, maxChunkY = MIN(level->depth,  (y + 1) * CHUNK_SIZE);
        int minChunkZ = z * CHUNK_SIZE, maxChunkZ = MIN(level->height, (z + 1) * CHUNK_SIZE);
        int idx = (x + y * r->chunkAmountX) * r->chunkAmountZ + z;
        Chunk_init(&r->chunks[idx], level, terrainTex, minChunkX, minChunkY, minChunkZ, maxChunkX, maxChunkY, maxChunkZ);
        r->sortedChunks[idx] = &r->chunks[idx];
    }

    r->drawDistance = 0;
    // force an immediate distance sort on the first render() call
    r->lastSortX = r->lastSortY = r->lastSortZ = -900000.0;

    r->surroundLists = glGenLists(2);
    glNewList(r->surroundLists + 0, GL_COMPILE);
    compileSurroundingGround(r);
    glEndList();
    glNewList(r->surroundLists + 1, GL_COMPILE);
    compileSurroundingWater(r);
    glEndList();

    r->cloudTick = 0;
    r->cloudTexture = loadTexture("resources/clouds.png", GL_NEAREST);
}

void LevelRenderer_tick(LevelRenderer* r) {
    r->cloudTick++;
}

// c0.0.14a_08's fog end distance shrinks with the draw distance toggle:
// 1024/256/64/16 across the 4 states
float LevelRenderer_getFogEndDistance(const LevelRenderer* r) {
    return (float)(1024 >> (r->drawDistance * 2));
}

// two stacked planes: a scrolling textured cloud layer, and a plain sky
// colored ceiling above it, both tiled 512 units per cell out to a 2048
// unit skirt past the map edges same as the ground/water horizon
void LevelRenderer_renderClouds(LevelRenderer* r, float partialTicks) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, r->cloudTexture);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    const float texScale = 4.8828125e-4f; // 1/2048
    const float y = (float)(r->level->depth + 2);
    const float uOffset = (r->cloudTick + partialTicks) * texScale * 0.03f;

    Tessellator_begin(&TESSELLATOR);
    for (int xx = -2048; xx < r->level->width + 2048; xx += 512) {
        for (int zz = -2048; zz < r->level->height + 2048; zz += 512) {
            float u0 = xx * texScale + uOffset, u1 = (xx + 512) * texScale + uOffset;
            float v0 = zz * texScale, v1 = (zz + 512) * texScale;

            Tessellator_vertexUV(&TESSELLATOR, (float)xx,       y, (float)(zz + 512), u0, v1);
            Tessellator_vertexUV(&TESSELLATOR, (float)(xx+512), y, (float)(zz + 512), u1, v1);
            Tessellator_vertexUV(&TESSELLATOR, (float)(xx+512), y, (float)zz,         u1, v0);
            Tessellator_vertexUV(&TESSELLATOR, (float)xx,       y, (float)zz,         u0, v0);

            Tessellator_vertexUV(&TESSELLATOR, (float)xx,       y, (float)zz,         u0, v0);
            Tessellator_vertexUV(&TESSELLATOR, (float)(xx+512), y, (float)zz,         u1, v0);
            Tessellator_vertexUV(&TESSELLATOR, (float)(xx+512), y, (float)(zz + 512), u1, v1);
            Tessellator_vertexUV(&TESSELLATOR, (float)xx,       y, (float)(zz + 512), u0, v1);
        }
    }
    Tessellator_end(&TESSELLATOR);
    glDisable(GL_TEXTURE_2D);

    Tessellator_begin(&TESSELLATOR);
    Tessellator_color(&TESSELLATOR, 0.5f, 0.8f, 1.0f);
    const float y2 = (float)(r->level->depth + 10);
    for (int xx = -2048; xx < r->level->width + 2048; xx += 512) {
        for (int zz = -2048; zz < r->level->height + 2048; zz += 512) {
            Tessellator_vertex(&TESSELLATOR, (float)xx,       y2, (float)zz);
            Tessellator_vertex(&TESSELLATOR, (float)(xx+512), y2, (float)zz);
            Tessellator_vertex(&TESSELLATOR, (float)(xx+512), y2, (float)(zz + 512));
            Tessellator_vertex(&TESSELLATOR, (float)xx,       y2, (float)(zz + 512));
        }
    }
    Tessellator_end(&TESSELLATOR);
}

void LevelRenderer_cull(LevelRenderer* r, const Frustum* frustum_) {
    int total = r->chunkAmountX * r->chunkAmountY * r->chunkAmountZ;
    for (int i = 0; i < total; ++i) {
        r->chunks[i].visible = frustum_isVisible(frustum_, &r->chunks[i].boundingBox) ? true : false;
    }
}

void LevelRenderer_toggleDrawDistance(LevelRenderer* r, bool reverse) {
    // c0.0.17a: reverse cycles when either Shift key is held, using & 3
    // instead of % 4 so decrementing from 0 wraps to 3 correctly
    r->drawDistance = (r->drawDistance + (reverse ? -1 : 1)) & 0x3;
}

void LevelRenderer_renderSurroundingGround(const LevelRenderer* r) {
    glCallList(r->surroundLists + 0);
}

void LevelRenderer_renderSurroundingWater(const LevelRenderer* r) {
    glCallList(r->surroundLists + 1);
}

static const Player* gDistPlayer = NULL;
static int distance_cmp(const void* a, const void* b) {
    const Chunk* c1 = *(const Chunk* const*)a;
    const Chunk* c2 = *(const Chunk* const*)b;
    return (Chunk_distanceToSqr(c1, gDistPlayer) < Chunk_distanceToSqr(c2, gDistPlayer)) ? -1 : 1;
}

void LevelRenderer_render(LevelRenderer* r, const Player* player, int layer) {
    int total = r->chunkAmountX * r->chunkAmountY * r->chunkAmountZ;

    double xd = player->e.x - r->lastSortX;
    double yd = player->e.y - r->lastSortY;
    double zd = player->e.z - r->lastSortZ;
    if (xd * xd + yd * yd + zd * zd > 64.0) {
        r->lastSortX = player->e.x;
        r->lastSortY = player->e.y;
        r->lastSortZ = player->e.z;
        gDistPlayer = player;
        qsort(r->sortedChunks, (size_t)total, sizeof(Chunk*), distance_cmp);
    }

    for (int i = 0; i < total; ++i) {
        Chunk* c = r->sortedChunks[i];
        if (!c->visible) continue;
        double dd = (double)(256 / (1 << r->drawDistance));
        if (r->drawDistance == 0 || Chunk_distanceToSqr(c, player) < dd * dd) {
            Chunk_render(c, layer);
        }
    }
}

void LevelRenderer_destroy(LevelRenderer* r) {
    int total = r->chunkAmountX * r->chunkAmountY * r->chunkAmountZ;
    for (int i = 0; i < total; ++i) {
        Chunk_destroy(&r->chunks[i]);
    }
    glDeleteLists(r->surroundLists, 2);

    free(r->chunks);
    free(r->sortedChunks);
    free(r->dirtyScratch);
}

/* dirty chunk prioritization */

// sort state for qsort comparator
static const Player* gSortPlayer = NULL;

// matches c0.0.13a's simplified DirtyChunkSorter: visible chunks first, using
// the flag LevelRenderer_cull already set this frame, then nearest first.
// The old dirtiedTime staleness tiebreak was dropped in this version.
static int dirty_cmp(const void* a, const void* b) {
    const Chunk* c1 = *(const Chunk* const*)a;
    const Chunk* c2 = *(const Chunk* const*)b;

    if (c1 == c2) return 0;

    if (c1->visible && !c2->visible) return -1;
    if (c2->visible && !c1->visible) return  1;

    double dist1 = Chunk_distanceToSqr(c1, gSortPlayer);
    double dist2 = Chunk_distanceToSqr(c2, gSortPlayer);
    return (dist1 < dist2) ? -1 : 1;
}

int LevelRenderer_updateDirtyChunks(LevelRenderer* r, const Player* player) {
    // collect dirty chunk pointers into a reused scratch buffer, avoiding a
    // malloc and free every frame regardless of how many chunks are dirty
    int total = r->chunkAmountX * r->chunkAmountY * r->chunkAmountZ;
    Chunk** list = r->dirtyScratch;

    int n = 0;
    for (int i = 0; i < total; ++i) {
        if (Chunk_isDirty(&r->chunks[i])) list[n++] = &r->chunks[i];
    }
    if (n == 0) return 0;

    // sort with priorities
    gSortPlayer = player;
    qsort(list, (size_t)n, sizeof(Chunk*), dirty_cmp);

    // rebuild up to 4 per frame (c0.0.13a halved MAX_REBUILDS_PER_FRAME from 8)
    int limit = n < 4 ? n : 4;
    for (int i = 0; i < limit; ++i) {
        Chunk_rebuild(list[i], 0); // solid, single pass since c0.0.14a_08
        Chunk_rebuild(list[i], 1); // liquid, renumbered down from 2
        Chunk_rebuild(list[i], 2); // c0.0.20a_02: unlit cross-quad plants, split out of the liquid list
    }

    return limit;
}

/*
   mode 0 is additive pulsing, only the cube faces facing the player
   mode 1 is alpha blended, a textured preview block on the adjacent cell,
   rendered in both layers (0 and 1)
*/
void LevelRenderer_renderHit(LevelRenderer* r, const Player* player, HitResult* h, int mode, int tileId) {
    if (!h) return;

    if (mode == 0) {
        // destroy highlight: additive, pulsing, only faces facing the player
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        float a = (float)(sin((double)currentTimeMillis() / 100.0) * 0.2 + 0.4) * 0.5f;
        glColor4f(1.f, 1.f, 1.f, a);

        Tessellator_begin(&TESSELLATOR);
        for (int face = 0; face < 6; ++face) {
            Face_render(&TESSELLATOR, h->x, h->y, h->z, face, player->e.x, player->e.y, player->e.z);
        }
        Tessellator_end(&TESSELLATOR);

        glDisable(GL_BLEND);
        return;
    }

    // place preview: alpha blend, pulsing tint and alpha on adjacent cell
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float br = (float)(sin((double)currentTimeMillis() / 100.0) * 0.2 + 0.8);
    float al = (float)(sin((double)currentTimeMillis() / 200.0) * 0.2 + 0.5);
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

        Tessellator_setIgnoreColor(&TESSELLATOR, 1); // Java: t.noColor()
        Tessellator_begin(&TESSELLATOR);
        t->render(t, &TESSELLATOR, r->level, 0, x, y, z);
        t->render(t, &TESSELLATOR, r->level, 1, x, y, z);
        Tessellator_end(&TESSELLATOR);
        Tessellator_setIgnoreColor(&TESSELLATOR, 0);

        glDisable(GL_TEXTURE_2D);
    }

    glDisable(GL_BLEND);
}

// c0.0.13a addition: literal thin black wireframe box around the targeted
// block/placement cell (separate from the pulsing tint in LevelRenderer_renderHit).
void LevelRenderer_renderHitOutline(HitResult* h, int mode) {
    if (!h) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.4f);

    float x = (float)h->x, y = (float)h->y, z = (float)h->z;
    if (mode == 1) {
        switch (h->f) {
            case 0: y -= 1.0f; break;
            case 1: y += 1.0f; break;
            case 2: z -= 1.0f; break;
            case 3: z += 1.0f; break;
            case 4: x -= 1.0f; break;
            case 5: x += 1.0f; break;
        }
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

void levelRenderer_allChanged(Level* level, LevelRenderer* r) {
    LevelRenderer_setDirty(r, 0, 0, 0, level->width, level->depth, level->height);
}
