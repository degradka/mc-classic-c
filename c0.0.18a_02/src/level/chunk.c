// level/chunk.c

#include "chunk.h"
#include "tile/tile.h"
#include "../player.h"
#include <math.h>

static Tessellator TESSELLATOR;

int Chunk_updates = 0;

static void reset(Chunk* c) {
    for (int i = 0; i < 2; i++) {
        glNewList(c->lists + i, GL_COMPILE);
        glEndList();
    }
}

// the renderer hands out two lists from its block, they get emptied instead of deleted
void Chunk_init(Chunk* c, Level* level, int x, int y, int z, int lists) {
    c->level = level;
    c->x = x;
    c->y = y;
    c->z = z;
    c->empty[0] = c->empty[1] = false;
    c->visible = false;
    c->dirty = false;
    c->lists = lists;
    reset(c);
}

void Chunk_destroy(Chunk* c) {
    reset(c);
    c->level = NULL;
}

float Chunk_distanceToSqr(const Chunk* c, const Player* p) {
    float xd = p->e.x - (float)c->x;
    float yd = p->e.y - (float)c->y;
    float zd = p->e.z - (float)c->z;
    return xd * xd + yd * yd + zd * zd;
}

void Chunk_rebuild(Chunk* c) {
    Chunk_updates++;
    for (int layer = 0; layer < 2; layer++) {
        glNewList(c->lists + layer, GL_COMPILE);
        Tessellator_begin(&TESSELLATOR);
        bool drawn = false;
        for (int x = c->x; x < c->x + 16; x++)
        for (int y = c->y; y < c->y + 16; y++)
        for (int z = c->z; z < c->z + 16; z++) {
            int id = Level_getTile(c->level, x, y, z);
            if (id > 0) drawn |= gTiles[id]->render(gTiles[id], &TESSELLATOR, c->level, layer, x, y, z);
        }
        Tessellator_end(&TESSELLATOR);
        glEndList();
        c->empty[layer] = !drawn;
    }
}

void Chunk_addList(const Chunk* c, unsigned int* lists, int* count, int layer) {
    if (!c->visible || c->empty[layer]) return;
    lists[(*count)++] = (unsigned int)(c->lists + layer);
}

void Chunk_cull(Chunk* c, const Frustum* frustum) {
    AABB box = AABB_create((float)c->x, (float)c->y, (float)c->z, (float)(c->x + 16), (float)(c->y + 16), (float)(c->z + 16));
    c->visible = frustum_isVisible(frustum, &box) ? true : false;
}
