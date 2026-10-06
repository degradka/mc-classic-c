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
    c->dirty = true;
    c->dirtiedTime = currentTimeMillis();
    c->visible = false;

    // distance checks use the chunk's min corner
    c->x = minX;
    c->y = minY;
    c->z = minZ;

    c->boundingBox = AABB_create(minX, minY, minZ, maxX, maxY, maxZ);
    // 0 solid, 1 liquid, 2 plants
    // plants have their own list so the liquid depth pass doesn't cut their crossed quads
    c->lists = glGenLists(3);
}

void Chunk_destroy(Chunk* c) {
    glDeleteLists(c->lists, 3);
}

void Chunk_rebuild(Chunk* c, int layer) {
    c->dirty = false;

    glNewList(c->lists + layer, GL_COMPILE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, c->texture);
    Tessellator_begin(&TESSELLATOR);

    int tiles = 0;
    // (coarse timing)
    long long nsStart = getCurrentTimeInNanoseconds();

    static const int dir[6][3] = { { 0, -1, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 }, { -1, 0, 0 }, { 1, 0, 0 } };
    static const float shade[6] = { 0.5f, 1.0f, 0.6f, 0.6f, 0.8f, 0.8f };
    if (layer == 0) {
        for (int face = 0; face < 6; ++face) {
            for (int y = c->minY; y < c->maxY; ++y)
            for (int z = c->minZ; z < c->maxZ; ++z)
            for (int x = c->minX; x < c->maxX; ++x) {
                int id = Level_getTile(c->level, x, y, z);
                if (id <= 0 || !gTiles[id] || !gTiles[id]->isCube) continue;
                int nx = x + dir[face][0], ny = y + dir[face][1], nz = z + dir[face][2];
                int nid = Level_getTile(c->level, nx, ny, nz);
                if (nid > 0 && gTiles[nid] && gTiles[nid]->isSolid(gTiles[nid])) continue;
                float b = shade[face] * Level_getBrightness(c->level, nx, ny, nz);
                Tessellator_color(&TESSELLATOR, b, b, b);
                Tile_renderCubeFace(gTiles[id], &TESSELLATOR, x, y, z, face);
                tiles++;
            }
        }
    } else {
        for (int y = c->minY; y < c->maxY; ++y)
        for (int z = c->minZ; z < c->maxZ; ++z)
        for (int x = c->minX; x < c->maxX; ++x) {
            int id = Level_getTile(c->level, x, y, z);
            if (id <= 0 || !gTiles[id] || gTiles[id]->isCube) continue;
            const Tile* t = gTiles[id];
            bool liquid = t->liquidType != LIQUID_NONE;
            if (layer == 1 && liquid) t->render(t, &TESSELLATOR, c->level, 1, x, y, z);
            if (layer == 2 && !liquid) t->render(t, &TESSELLATOR, c->level, 0, x, y, z);
            tiles++;
        }
    }

    Tessellator_end(&TESSELLATOR);
    glDisable(GL_TEXTURE_2D);
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

double Chunk_distanceToSqr(const Chunk* c, const Player* p) {
    double dx = p->e.x - c->x;
    double dy = p->e.y - c->y;
    double dz = p->e.z - c->z;
    return dx*dx + dy*dy + dz*dz;
}