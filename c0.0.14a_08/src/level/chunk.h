// level/chunk.h: one 16x16x16 piece of the level in two display lists

#ifndef CHUNK_H
#define CHUNK_H

#include <stdbool.h>
#include "level.h"
#include "../renderer/tessellator.h"
#include "../renderer/frustum.h"
#include "../phys/aabb.h"

struct Level; typedef struct Level Level;
struct Player; typedef struct Player Player;

typedef struct Chunk {
    Level* level;
    int x, y, z;
    int lists;
    // a layer drew nothing at the last rebuild
    bool empty[2];
    bool visible;
    // in the level renderer's dirty set
    bool dirty;
} Chunk;

extern int Chunk_updates;

void  Chunk_init(Chunk* chunk, Level* level, int x, int y, int z);
void  Chunk_destroy(Chunk* chunk);
void  Chunk_rebuild(Chunk* chunk);
// adds the layer's list when the chunk is visible and the layer isn't empty
void  Chunk_addList(const Chunk* chunk, unsigned int* lists, int* count, int layer);
void  Chunk_cull(Chunk* chunk, const Frustum* frustum);
// distance to the chunk's corner, not its center
float Chunk_distanceToSqr(const Chunk* c, const Player* p);

#endif  // CHUNK_H
