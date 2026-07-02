// level/tile/tile.h — tile registry, per-face textures, render helpers

#ifndef TILE_H
#define TILE_H

#include "../level.h"
#include "../../renderer/tessellator.h"

struct ParticleEngine; typedef struct ParticleEngine ParticleEngine;

typedef struct Tile Tile;

// tile.getLiquidType() constants
#define LIQUID_NONE  0
#define LIQUID_WATER 1
#define LIQUID_LAVA  2

struct Tile {
    int id;
    int textureId;
    int liquidType; // LIQUID_NONE / LIQUID_WATER / LIQUID_LAVA

    int  (*getTexture)(const Tile* self, int face);
    void (*render)(const Tile* self, Tessellator* t, const Level* lvl, int layer, int x, int y, int z);
    void (*onTick)(const Tile* self, Level* lvl, int x, int y, int z);

    int  (*isSolid)(const Tile* self);
    int  (*blocksLight)(const Tile* self);
    // return 1 and fill *out on success; return 0 if no collision box
    int  (*getAABB)(const Tile* self, int x, int y, int z, AABB* out);
};

// Global registry, index by tile id (0..255)
extern const Tile* gTiles[256];

// Predefined tiles
extern Tile TILE_ROCK;      // id=1
extern Tile TILE_GRASS;     // id=2 (custom per face)
extern Tile TILE_DIRT;      // id=3
extern Tile TILE_STONEBRICK;// id=4
extern Tile TILE_WOOD;      // id=5
extern Tile TILE_BUSH;      // id=6

void Tile_registerAll(void);

// Helper to render a plain, untextured face (for highlights)
void Face_render(Tessellator* t, int x, int y, int z, int face);

void Tile_onDestroy(const Tile* self, Level* lvl, int x, int y, int z, ParticleEngine* engine);

#endif