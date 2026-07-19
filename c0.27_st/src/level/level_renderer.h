// level_renderer.h: chunk grid, frustum culling, dirty marking, and hit highlight

#ifndef LEVELRENDERER_H
#define LEVELRENDERER_H

#include "../hitresult.h"
#include "../renderer/frustum.h"
#include <stdbool.h>

struct Level;     typedef struct Level Level;
struct Chunk;     typedef struct Chunk Chunk;
struct HitResult; // forward

struct Level;   typedef struct Level Level;
struct Chunk;   typedef struct Chunk Chunk;
struct Player;  typedef struct Player Player;

#define CHUNK_SIZE 16

typedef struct LevelRenderer {
    Chunk*      chunks;
    Chunk**     sortedChunks; // pointers into chunks[], kept sorted by distance
    Chunk**     dirtyScratch; // reused each frame instead of malloc per frame
    int         chunkAmountX, chunkAmountY, chunkAmountZ;

    Level*      level;
    int         terrainTex;

    int         drawDistance; // 0..3, 0 = unlimited
    double      lastSortX, lastSortY, lastSortZ; // player pos at the last sort

    unsigned int surroundLists; // glGenLists(2) base: +0 ground, +1 water

    // c0.0.14a_08: new cloud layer, scrolls once per game tick
    int         cloudTick;
    int         cloudTexture;
} LevelRenderer;

void LevelRenderer_init(LevelRenderer* renderer, Level* level, int terrainTex);
void LevelRenderer_render(LevelRenderer* renderer, const Player* player, int layer);
void LevelRenderer_destroy(LevelRenderer* renderer);

void LevelRenderer_cull(LevelRenderer* renderer, const Frustum* frustum);
float LevelRenderer_getFogEndDistance(const LevelRenderer* renderer);
void LevelRenderer_renderSurroundingGround(const LevelRenderer* renderer);
void LevelRenderer_renderSurroundingWater(const LevelRenderer* renderer);
void LevelRenderer_tick(LevelRenderer* renderer);
void LevelRenderer_renderClouds(LevelRenderer* renderer, float partialTicks);

void LevelRenderer_setDirty(const LevelRenderer* renderer, int minX, int minY, int minZ, int maxX, int maxY, int maxZ);
void levelRenderer_tileChanged(LevelRenderer* renderer, int x, int y, int z);
void levelRenderer_lightColumnChanged(LevelRenderer* renderer, int x, int z, int minY, int maxY);
void levelRenderer_allChanged(Level* level, LevelRenderer* renderer);

// digFraction is the current mining progress (0..1, see updateMining in
// minecraft.c); draws the real source's 10 frame crack overlay, and only
// that; no-op while not actively mining (digFraction<=0). CORRECTION: no
// more mode/player/tileId parameters, since real source's own render-hit method
// never draws anything else (confirmed via direct read + javap on the real
// call site, which always passes a hardcoded mode of 0)
void LevelRenderer_renderHit(LevelRenderer* renderer, struct HitResult* h, float digFraction);
void LevelRenderer_renderHitOutline(const LevelRenderer* renderer, struct HitResult* h);

int LevelRenderer_updateDirtyChunks(LevelRenderer* r, const Player* player);

#endif  // LEVELRENDERER_H
