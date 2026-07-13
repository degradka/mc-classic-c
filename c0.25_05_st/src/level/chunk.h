// chunk.h: chunk display lists, rebuild and render

#ifndef CHUNK_H
#define CHUNK_H

#include <stdbool.h>
#include "level.h"
#include "../renderer/tessellator.h"
#include "../phys/aabb.h"

struct Level; typedef struct Level Level;
struct Player; typedef struct Player Player;

typedef struct Chunk {
    Level* level;
    int    texture;

    AABB   boundingBox;

    int minX, minY, minZ;
    int maxX, maxY, maxZ;

    int  lists;
    bool dirty;
    long long dirtiedTime;
    bool visible; // set once per frame by LevelRenderer_cull

    double x, y, z;
} Chunk;

void Chunk_init(Chunk* chunk, Level* level, GLuint texture, int minX, int minY, int minZ, int maxX, int maxY, int maxZ);
void Chunk_destroy(Chunk* chunk);
void Chunk_rebuild(Chunk* chunk, int layer);
void Chunk_render(Chunk* chunk, int layer);
void Chunk_setDirty(Chunk* chunk);

static inline bool Chunk_isDirty(const Chunk* c) { return c->dirty; }
double Chunk_distanceToSqr(const Chunk* c, const Player* p);

#endif  // CHUNK_H