// level/chunk.c

#include "chunk.h"
#include "tile/tile.h"
#include "../timer.h"
#include "../player.h"
#include <math.h>

static Tessellator TESSELLATOR;

static long long CHUNK_TotalTimeNS  = 0;
static int       CHUNK_TotalUpdates = 0;
int Chunk_updates = 0;

void Chunk_init(Chunk* c, Level* level, int minX, int minY, int minZ, int maxX, int maxY, int maxZ) {
    c->level = level;

    c->minX = minX; c->minY = minY; c->minZ = minZ;
    c->maxX = maxX; c->maxY = maxY; c->maxZ = maxZ;
    c->dirty = true;
    c->dirtiedTime = 0;
    c->visible = false;

    c->x = (float)(minX + maxX) / 2.0f;
    c->y = (float)(minY + maxY) / 2.0f;
    c->z = (float)(minZ + maxZ) / 2.0f;

    c->boundingBox = AABB_create((float)minX, (float)minY, (float)minZ, (float)maxX, (float)maxY, (float)maxZ);
    c->lists = glGenLists(3);
}

void Chunk_destroy(Chunk* c) {
    glDeleteLists(c->lists, 3);
}

static void Chunk_rebuildLayer(Chunk* c, int layer) {
    long long nsStart = getCurrentTimeInNanoseconds();
    glNewList(c->lists + layer, GL_COMPILE);
    Tessellator_begin(&TESSELLATOR);

    int tiles = 0;
    for (int x = c->minX; x < c->maxX; ++x)
    for (int y = c->minY; y < c->maxY; ++y)
    for (int z = c->minZ; z < c->maxZ; ++z) {
        int tileId = Level_getTile(c->level, x, y, z);
        if (tileId > 0) {
            gTiles[tileId]->render(gTiles[tileId], &TESSELLATOR, c->level, layer, x, y, z);
            tiles++;
        }
    }

    Tessellator_end(&TESSELLATOR);
    glEndList();

    if (tiles > 0) {
        CHUNK_TotalTimeNS += getCurrentTimeInNanoseconds() - nsStart;
        CHUNK_TotalUpdates++;
    }
}

void Chunk_rebuild(Chunk* c) {
    Chunk_updates++;
    Chunk_rebuildLayer(c, 0);
    Chunk_rebuildLayer(c, 1);
    Chunk_rebuildLayer(c, 2);
    c->dirty = false;
}

void Chunk_render(Chunk* c, int layer) {
    glCallList(c->lists + layer);
}

void Chunk_setDirty(Chunk* c) {
    if (!c->dirty) c->dirtiedTime = currentTimeMillis();
    c->dirty = true;
}

float Chunk_distanceToSqr(const Chunk* c, const Player* p) {
    float xd = p->e.x - c->x;
    float yd = p->e.y - c->y;
    float zd = p->e.z - c->z;
    return xd * xd + yd * yd + zd * zd;
}

void Chunk_reset(Chunk* c) {
    c->dirty = true;
    for (int i = 0; i < 3; i++) {
        glNewList(c->lists + i, GL_COMPILE);
        glEndList();
    }
}
