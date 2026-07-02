// level/level_renderer.h: chunk grid, culling, dirty chunks, and the hit highlight

#ifndef LEVELRENDERER_H
#define LEVELRENDERER_H

#include "../hitresult.h"

struct Level;     typedef struct Level Level;
struct Chunk;     typedef struct Chunk Chunk;
struct HitResult; // forward
struct Frustum;   typedef struct Frustum Frustum;

struct Level;   typedef struct Level Level;
struct Chunk;   typedef struct Chunk Chunk;
struct Player;  typedef struct Player Player;

#define CHUNK_SIZE 16

typedef struct LevelRenderer {
    Chunk*      chunks;
    Chunk**     sortedChunks;
    int         chunkAmountX, chunkAmountY, chunkAmountZ;

    Level*      level;
    int         terrainTex;

    int         drawDistance;
    double      lastSortX, lastSortY, lastSortZ; // player pos at last re-sort

    unsigned int surroundLists;
} LevelRenderer;

void LevelRenderer_init(LevelRenderer* renderer, Level* level, int terrainTex);
void LevelRenderer_render(LevelRenderer* renderer, const Player* player, int layer);
void LevelRenderer_destroy(LevelRenderer* renderer);

void LevelRenderer_cull(LevelRenderer* renderer, const Frustum* frustum);
void LevelRenderer_toggleDrawDistance(LevelRenderer* renderer);
void LevelRenderer_renderSurroundingGround(const LevelRenderer* renderer);
void LevelRenderer_renderSurroundingWater(const LevelRenderer* renderer);

void LevelRenderer_setDirty(const LevelRenderer* renderer, int minX, int minY, int minZ, int maxX, int maxY, int maxZ);
void levelRenderer_tileChanged(LevelRenderer* renderer, int x, int y, int z);
void levelRenderer_lightColumnChanged(LevelRenderer* renderer, int x, int z, int minY, int maxY);
void levelRenderer_allChanged(Level* level, LevelRenderer* renderer);

void LevelRenderer_renderHit(LevelRenderer* renderer, struct HitResult* h, int mode, int tileId);
void LevelRenderer_renderHitOutline(struct HitResult* h, int mode);

int LevelRenderer_updateDirtyChunks(LevelRenderer* r, const Player* player);

#endif  // LEVELRENDERER_H
