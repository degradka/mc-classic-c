// level/level_renderer.h: chunk grid, dirty chunks, surroundings, clouds and the hit highlight

#ifndef LEVELRENDERER_H
#define LEVELRENDERER_H

#include "../hitresult.h"
#include "../renderer/frustum.h"

struct Level;   typedef struct Level Level;
struct Chunk;   typedef struct Chunk Chunk;
struct Player;  typedef struct Player Player;

typedef struct LevelRenderer {
    Level*      level;
    // fixed grid order
    Chunk*      grid;
    // the same chunks, sorted by distance when the player moves far enough
    Chunk**     chunks;
    int         xChunks, yChunks, zChunks;
    unsigned int* callLists;
    int         callListCount;

    int         terrainTex;
    int         rockTex;
    int         waterTex;
    int         cloudsTex;

    int         drawDistance;
    // counts ticks, moves the clouds
    int         cloudTickCounter;
    float       lX, lY, lZ;

    unsigned int surroundLists;
    // two lists for every chunk, made once
    unsigned int chunkLists;
} LevelRenderer;

void LevelRenderer_init(LevelRenderer* renderer, int terrainTex);
void LevelRenderer_setLevel(LevelRenderer* renderer, Level* level);
void LevelRenderer_allChanged(LevelRenderer* renderer);
int  LevelRenderer_render(LevelRenderer* renderer, const Player* player, int layer);
void LevelRenderer_renderAgain(LevelRenderer* renderer);
void LevelRenderer_renderEntities(LevelRenderer* renderer, const Frustum* frustum, float a);
void LevelRenderer_renderClouds(LevelRenderer* renderer, float a);
// the camera is in a solid tile, draw it dark from both sides
void LevelRenderer_renderInside(LevelRenderer* renderer, int x, int y, int z);
void LevelRenderer_destroy(LevelRenderer* renderer);

void LevelRenderer_cull(LevelRenderer* renderer, const Frustum* frustum);
void LevelRenderer_updateDirtyChunks(LevelRenderer* renderer, const Player* player);
void LevelRenderer_renderSurroundingGround(const LevelRenderer* renderer);
void LevelRenderer_renderSurroundingWater(const LevelRenderer* renderer);

void LevelRenderer_setDirty(LevelRenderer* renderer, int x0, int y0, int z0, int x1, int y1, int z1);
void levelRenderer_tileChanged(LevelRenderer* renderer, int x, int y, int z);
void levelRenderer_lightColumnChanged(LevelRenderer* renderer, int x, int z, int minY, int maxY);

void LevelRenderer_renderHit(LevelRenderer* renderer, const Player* player, const HitResult* h, int mode, int tileType);
void LevelRenderer_renderHitOutline(const HitResult* h, int mode);

#endif  // LEVELRENDERER_H
