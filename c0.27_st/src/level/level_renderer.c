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
#include "../renderer/tessellator.h"
#include <math.h>
#include <stdio.h>

static Tessellator TESSELLATOR;

extern Tessellator TESSELLATOR;

// the ground and water around the map
// list 0 is ground and list 1 is water
static void compileSurroundingGround(LevelRenderer* r) {
    glEnable(GL_TEXTURE_2D);
    // no mipmaps anymore, every texture goes up with plain glTexImage2D
    GLuint rockTex = loadTexture("resources/rock.png", GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, rockTex);
    glColor4f(0.5f, 0.5f, 0.5f, 1.0f);

    const float y = Level_getGroundLevel(r->level);
    int s = 128;
    if (s > r->level->width)  s = r->level->width;
    if (s > r->level->height) s = r->level->height;
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
}

static void compileSurroundingWater(LevelRenderer* r) {
    glEnable(GL_TEXTURE_2D);
    glColor3f(1.0f, 1.0f, 1.0f);
    // no mipmaps, like compileSurroundingGround
    GLuint waterTex = loadTexture("resources/water.png", GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, waterTex);

    const float y = Level_getWaterLevel(r->level);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int s = 128;
    if (s > r->level->width)  s = r->level->width;
    if (s > r->level->height) s = r->level->height;
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
    // sorts on the first render
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

float LevelRenderer_getFogEndDistance(const LevelRenderer* r) {
    return (float)(512 >> (r->drawDistance << 1));
}

static void anaglyphColor(float* r, float* g, float* b) {
    float nr = (*r * 30.0f + *g * 59.0f + *b * 11.0f) / 100.0f;
    float ng = (*r * 30.0f + *g * 70.0f) / 100.0f;
    float nb = (*r * 30.0f + *b * 70.0f) / 100.0f;
    *r = nr; *g = ng; *b = nb;
}

void LevelRenderer_renderClouds(LevelRenderer* r, float partialTicks, bool anaglyph) {
    glBindTexture(GL_TEXTURE_2D, r->cloudTexture);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    float cr = (float)(r->level->cloudColor >> 16 & 0xFF) / 255.0f;
    float cg = (float)(r->level->cloudColor >> 8 & 0xFF) / 255.0f;
    float cb = (float)(r->level->cloudColor & 0xFF) / 255.0f;
    if (anaglyph) anaglyphColor(&cr, &cg, &cb);

    const float texScale = 4.8828125e-4f; // 1/2048
    const float y = (float)(r->level->depth + 2);
    const float uOffset = (r->cloudTick + partialTicks) * texScale * 0.03f;

    Tessellator_begin(&TESSELLATOR);
    Tessellator_color(&TESSELLATOR, cr, cg, cb);
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
    float sr = (float)(r->level->skyColor >> 16 & 0xFF) / 255.0f;
    float sg = (float)(r->level->skyColor >> 8 & 0xFF) / 255.0f;
    float sb = (float)(r->level->skyColor & 0xFF) / 255.0f;
    if (anaglyph) anaglyphColor(&sr, &sg, &sb);
    Tessellator_color(&TESSELLATOR, sr, sg, sb);
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
    glEnable(GL_TEXTURE_2D);
}

void LevelRenderer_renderInside(const LevelRenderer* r, int x, int y, int z) {
    int id = Level_getTile(r->level, x, y, z);
    const Tile* t = id > 0 && id < 256 ? gTiles[id] : NULL;
    if (!t || !t->isSolid(t)) return;
    glColor4f(0.2f, 0.2f, 0.2f, 1.0f);
    glDepthFunc(GL_LESS);
    Tessellator_begin(&TESSELLATOR);
    for (int face = 0; face < 6; ++face) t->renderFace(t, &TESSELLATOR, x, y, z, face);
    Tessellator_end(&TESSELLATOR);
    glCullFace(GL_FRONT);
    Tessellator_begin(&TESSELLATOR);
    for (int face = 0; face < 6; ++face) t->renderFace(t, &TESSELLATOR, x, y, z, face);
    Tessellator_end(&TESSELLATOR);
    glCullFace(GL_BACK);
    glDepthFunc(GL_LEQUAL);
}

void LevelRenderer_cull(LevelRenderer* r, const Frustum* frustum_) {
    int total = r->chunkAmountX * r->chunkAmountY * r->chunkAmountZ;
    for (int i = 0; i < total; ++i) {
        r->chunks[i].visible = frustum_isVisible(frustum_, &r->chunks[i].boundingBox) ? true : false;
    }
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

    // pass 0 is list 0, cube faces and plants together
    bool any = false;
    for (int i = 0; i < total && !any; ++i) {
        const Chunk* c = r->sortedChunks[i];
        if (!c->visible) continue;
        if (layer == 0) any = !c->empty[0] || !c->empty[2];
        else any = !c->empty[1];
    }
    if (!any) return;
    glBindTexture(GL_TEXTURE_2D, r->terrainTex);
    for (int i = 0; i < total; ++i) {
        Chunk* c = r->sortedChunks[i];
        if (!c->visible) continue;
        if (layer == 0) {
            if (!c->empty[0]) Chunk_render(c, 0);
            if (!c->empty[2]) Chunk_render(c, 2);
        } else if (!c->empty[1]) {
            Chunk_render(c, 1);
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

// DirtyChunkSorter, visible chunks first, then the nearest
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
    // dirty chunks go into a reused buffer, no malloc every frame
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

    // rebuild up to 4 a frame
    int limit = n < 3 ? n : 3;
    for (int i = 0; i < limit; ++i) {
        Chunk_rebuild(list[i], 0); // solid
        Chunk_rebuild(list[i], 1); // liquid
        Chunk_rebuild(list[i], 2); // plants
    }

    return limit;
}

// the mining cracks, the hit tile's shape a bit bigger with a darkening blend
static void renderDigOverlay(const Level* level, int terrainTex, int x, int y, int z, float fraction) {
    glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
    glBindTexture(GL_TEXTURE_2D, terrainTex);
    glColor4f(1.f, 1.f, 1.f, 0.5f);

    glPushMatrix();
    int id = Level_getTile(level, x, y, z);
    const Tile* tile = id > 0 ? gTiles[id] : NULL;
    if (!tile) tile = &TILE_ROCK;
    float cx = (tile->xx0 + tile->xx1) / 2.0f, cy = (tile->yy0 + tile->yy1) / 2.0f, cz = (tile->zz0 + tile->zz1) / 2.0f;
    glTranslatef((float)x + cx, (float)y + cy, (float)z + cz);
    glScalef(1.01f, 1.01f, 1.01f);
    glTranslatef(-((float)x + cx), -((float)y + cy), -((float)z + cz));

    Tessellator_begin(&TESSELLATOR);
    glDepthMask(GL_FALSE);
    for (int face = 0; face < 6; ++face) {
        Tile_renderFaceTex(tile, &TESSELLATOR, x, y, z, face, 240 + (int)(fraction * 10.0f));
    }
    Tessellator_end(&TESSELLATOR);
    glDepthMask(GL_TRUE);

    glPopMatrix();
}

void LevelRenderer_renderHit(LevelRenderer* r, const HitResult* h, float digFraction) {
    glEnable(GL_BLEND);
    glEnable(GL_ALPHA_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(1.0f, 1.0f, 1.0f, (float)(sin((double)currentTimeMillis() / 100.0) * 0.2 + 0.4) * 0.5f);
    if (digFraction > 0.0f) {
        renderDigOverlay(r->level, r->terrainTex, h->x, h->y, h->z, digFraction);
    }
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
}

void LevelRenderer_renderHitOutline(const Level* level, const HitResult* h) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.4f);
    glLineWidth(2.0f);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);

    // the tile's shape, air would crash
    int id = Level_getTile(level, h->x, h->y, h->z);
    const Tile* t = id > 0 ? gTiles[id] : NULL;
    const float g = 0.002f;
    float x0 = (float)h->x - g, y0 = (float)h->y - g, z0 = (float)h->z - g;
    float x1 = (float)(h->x + 1) + g, y1 = (float)(h->y + 1) + g, z1 = (float)(h->z + 1) + g;
    if (t) {
        x0 = (float)h->x + t->xx0 - g; y0 = (float)h->y + t->yy0 - g; z0 = (float)h->z + t->zz0 - g;
        x1 = (float)h->x + t->xx1 + g; y1 = (float)h->y + t->yy1 + g; z1 = (float)h->z + t->zz1 + g;
    }

    glBegin(GL_LINE_STRIP);
    glVertex3f(x0, y0, z0);
    glVertex3f(x1, y0, z0);
    glVertex3f(x1, y0, z1);
    glVertex3f(x0, y0, z1);
    glVertex3f(x0, y0, z0);
    glEnd();

    glBegin(GL_LINE_STRIP);
    glVertex3f(x0, y1, z0);
    glVertex3f(x1, y1, z0);
    glVertex3f(x1, y1, z1);
    glVertex3f(x0, y1, z1);
    glVertex3f(x0, y1, z0);
    glEnd();

    glBegin(GL_LINES);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0);
    glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0);
    glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1);
    glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1);
    glEnd();

    glDepthMask(GL_TRUE);
    glEnable(GL_TEXTURE_2D);
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
