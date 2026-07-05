// level/tile/tile.h: tile registry, textures and render helpers

#ifndef TILE_H
#define TILE_H

#include "../level.h"
#include "../../renderer/tessellator.h"

struct ParticleEngine; typedef struct ParticleEngine ParticleEngine;

typedef struct Tile Tile;

// Tile.getLiquidType
#define LIQUID_NONE  0
#define LIQUID_WATER 1
#define LIQUID_LAVA  2

struct Tile {
    int id;
    int textureId;
    int liquidType; // LIQUID_NONE / LIQUID_WATER / LIQUID_LAVA

    // liquids only, the flowing and calm ids
    int tileId, calmTileId, spreadSpeed;

    // the shape, a full cube by default
    float xx0, yy0, zz0, xx1, yy1, zz1;

    int  (*getTexture)(const Tile* self, int face);
    // true when a face was drawn
    bool (*render)(const Tile* self, Tessellator* t, const Level* lvl, int layer, int x, int y, int z);
    void (*onTick)(const Tile* self, Level* lvl, int x, int y, int z);

    int  (*isSolid)(const Tile* self);
    int  (*blocksLight)(const Tile* self);
    // 0 when there is no collision box
    int  (*getAABB)(const Tile* self, int x, int y, int z, AABB* out);
    // false for liquids, you can't aim at them
    int  (*mayPick)(const Tile* self);

    void (*neighborChanged)(const Tile* self, Level* lvl, int x, int y, int z, int type);

    int  (*shouldRenderFace)(const Tile* self, const Level* lvl, int x, int y, int z, int layer, int face);
    void (*renderFace)(const Tile* self, Tessellator* t, int x, int y, int z, int face);
};

// indexed by tile id
extern const Tile* gTiles[256];

extern Tile TILE_ROCK;      // id=1
extern Tile TILE_GRASS;     // id=2
extern Tile TILE_DIRT;      // id=3
extern Tile TILE_STONEBRICK;// id=4
extern Tile TILE_WOOD;      // id=5
extern Tile TILE_BUSH;      // id=6
extern Tile TILE_UNBREAKABLE; // id=7

extern Tile TILE_WATER;      // id=8
extern Tile TILE_CALM_WATER; // id=9
extern Tile TILE_LAVA;       // id=10
extern Tile TILE_CALM_LAVA;  // id=11

void Tile_registerAll(void);

// Helper to render a plain, untextured face (for highlights)
void Face_render(Tessellator* t, int x, int y, int z, int face, float px, float py, float pz);

void Tile_onDestroy(const Tile* self, Level* lvl, int x, int y, int z, ParticleEngine* engine);

#endif