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

void Chunk_init(Chunk* c, Level* level, GLuint texture, int minX, int minY, int minZ, int maxX, int maxY, int maxZ) {
    c->level   = level;
    c->texture = texture;

    c->minX = minX; c->minY = minY; c->minZ = minZ;
    c->maxX = maxX; c->maxY = maxY; c->maxZ = maxZ;
    c->dirty = false;
    c->empty[0] = c->empty[1] = true;
    c->dirtiedTime = currentTimeMillis();
    c->visible = false;

    // distance checks use the chunk's min corner
    c->x = minX;
    c->y = minY;
    c->z = minZ;

    c->boundingBox = AABB_create(minX, minY, minZ, maxX, maxY, maxZ);
    // 0 is everything but water, 1 is water
    c->lists = glGenLists(2);
}

void Chunk_destroy(Chunk* c) {
    glDeleteLists(c->lists, 2);
}

// every tile draws itself into the list of its layer, x then y then z
// list 1 is only rebuilt when something in the chunk wanted it
void Chunk_rebuild(Chunk* c) {
    c->empty[0] = c->empty[1] = true;
    long long nsStart = getCurrentTimeInNanoseconds();
    for (int layer = 0; layer < 2; ++layer) {
        bool other = false;
        bool drew = false;
        glNewList(c->lists + layer, GL_COMPILE);
        Tessellator_begin(&TESSELLATOR);
        for (int x = c->minX; x < c->maxX; ++x)
        for (int y = c->minY; y < c->maxY; ++y)
        for (int z = c->minZ; z < c->maxZ; ++z) {
            int id = Level_getTile(c->level, x, y, z);
            if (id <= 0 || !gTiles[id]) continue;
            const Tile* t = gTiles[id];
            if (t->renderLayer != layer) {
                other = true;
                continue;
            }
            drew |= t->render(t, c->level, x, y, z, &TESSELLATOR);
        }
        Tessellator_end(&TESSELLATOR);
        glEndList();
        if (drew) c->empty[layer] = false;
        if (!other) break;
    }
    CHUNK_TotalTimeNS += (getCurrentTimeInNanoseconds() - nsStart);
    CHUNK_TotalUpdates++;
}

void Chunk_render(Chunk* c, int layer) {
    glCallList(c->lists + layer);
}

void Chunk_setDirty(Chunk* c) {
    if (!c->dirty) c->dirtiedTime = currentTimeMillis();
    c->dirty = true;
}

float Chunk_distanceToSqr(const Chunk* c, const Player* p) {
    float dx = p->e.x - (float)c->x;
    float dy = p->e.y - (float)c->y;
    float dz = p->e.z - (float)c->z;
    return dx * dx + dy * dy + dz * dz;
}