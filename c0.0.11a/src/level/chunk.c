// level/chunk.c

#include "chunk.h"
#include "tile/tile.h"
#include "../timer.h"
#include "../player.h"
#include <math.h>

static Tessellator TESSELLATOR;

extern Tessellator TESSELLATOR;

static long long CHUNK_TotalTimeNS  = 0;
static int       CHUNK_TotalUpdates = 0;
int Chunk_updates = 0;

void Chunk_init(Chunk* c, Level* level, int minX, int minY, int minZ, int maxX, int maxY, int maxZ) {
    c->level   = level;

    c->minX = minX; c->minY = minY; c->minZ = minZ;
    c->maxX = maxX; c->maxY = maxY; c->maxZ = maxZ;
    c->dirty = true;
    c->dirtiedTime = 0;

    c->x = (float)(minX + maxX) / 2.0f;
    c->y = (float)(minY + maxY) / 2.0f;
    c->z = (float)(minZ + maxZ) / 2.0f;

    c->boundingBox = AABB_create(minX, minY, minZ, maxX, maxY, maxZ);
    c->lists = glGenLists(2);
}

void Chunk_rebuild(Chunk* c, int layer) {
    c->dirty = false;
    Chunk_updates++;

    glNewList(c->lists + layer, GL_COMPILE);
    Tessellator_init(&TESSELLATOR);

    int tiles = 0;
    // (coarse timing)
    long long nsStart = getCurrentTimeInNanoseconds();

    for (int x = c->minX; x < c->maxX; ++x)
    for (int y = c->minY; y < c->maxY; ++y)
    for (int z = c->minZ; z < c->maxZ; ++z) {
        int tileId = Level_getTile(c->level, x, y, z);
        if (tileId > 0) {
            const Tile* t = gTiles[tileId];
            if (t && t->render) {
                t->render(t, &TESSELLATOR, c->level, layer, x, y, z);
                tiles++;
            }
        }
    }

    Tessellator_flush(&TESSELLATOR);
    glEndList();

    if (tiles > 0) {
        CHUNK_TotalTimeNS += (getCurrentTimeInNanoseconds() - nsStart);
        CHUNK_TotalUpdates++;
    }
}

void Chunk_render(Chunk* c, int layer) {
    glCallList(c->lists + layer);
}

void Chunk_setDirty(Chunk* c) {
    if (!c->dirty) c->dirtiedTime = currentTimeMillis();
    c->dirty = true;
}

float Chunk_distanceToSqr(const Chunk* c, const Player* p) {
    float dx = p->e.x - c->x;
    float dy = p->e.y - c->y;
    float dz = p->e.z - c->z;
    return dx * dx + dy * dy + dz * dz;
}