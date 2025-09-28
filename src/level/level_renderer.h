// level/level_renderer.h — chunk grid, frustum culling, dirty-marking, and hit highlight

#ifndef LEVELRENDERER_H
#define LEVELRENDERER_H

#include "../hitresult.h"

struct Level;     typedef struct Level Level;
struct Chunk;     typedef struct Chunk Chunk;
struct HitResult; // forward

struct Level;   typedef struct Level Level;
struct Chunk;   typedef struct Chunk Chunk;
struct Player;  typedef struct Player Player;

#define CHUNK_SIZE 16

typedef struct LevelRenderer {
    Chunk*      chunks;
    int         chunkAmountX, chunkAmountY, chunkAmountZ;

    Level*      level;
    int         terrainTex;

    int         drawDistance;
} LevelRenderer;

void LevelRenderer_init(LevelRenderer* renderer, Level* level, int terrainTex);
void LevelRenderer_render(const LevelRenderer* renderer, const Player* player, int layer);
void LevelRenderer_destroy(LevelRenderer* renderer);

void LevelRenderer_setDirty(const LevelRenderer* renderer, int minX, int minY, int minZ, int maxX, int maxY, int maxZ);
void LevelRenderer_tileChanged(LevelRenderer* renderer, int x, int y, int z);
void LevelRenderer_lightColumnChanged(LevelRenderer* renderer, int x, int z, int minY, int maxY);
void LevelRenderer_allChanged(Level* level, LevelRenderer* renderer);

void LevelRenderer_renderHit(LevelRenderer* renderer, struct HitResult* h, int mode, int tileId);
void LevelRenderer_renderHitOutline(struct Player* p, struct HitResult* h, int mode, int tileId);

int  LevelRenderer_updateDirtyChunks(LevelRenderer* r, const Player* player);

void LevelRenderer_toggleDrawDistance(LevelRenderer* r);

#endif  // LEVELRENDERER_H
