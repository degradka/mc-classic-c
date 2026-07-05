// tile.h: tile registry, per face textures, render helpers

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

    // Liquid only instance data, zero for regular tiles. Flowing and calm
    // variant ids, and how many tiles a spread can travel before stopping.
    int tileId, calmTileId, spreadSpeed;

    // Render shape, in block local coordinates. Default is a full cube
    // (0,0,0) to (1,1,1). Liquids crop the top so the surface sits slightly
    // below a full block.
    float xx0, yy0, zz0, xx1, yy1, zz1;

    int  (*getTexture)(const Tile* self, int face);
    void (*render)(const Tile* self, Tessellator* t, const Level* lvl, int layer, int x, int y, int z);
    void (*onTick)(const Tile* self, Level* lvl, int x, int y, int z);

    int  (*isSolid)(const Tile* self);
    int  (*blocksLight)(const Tile* self);
    // return 1 and fill *out on success; return 0 if no collision box
    int  (*getAABB)(const Tile* self, int x, int y, int z, AABB* out);
    // whether the tile can be targeted by the reach raycast at all. Liquids
    // are pass through here, so you cannot look at or break them.
    int  (*mayPick)(const Tile* self);

    // Reaction to a neighboring block changing to `type`. Default is a no op.
    void (*neighborChanged)(const Tile* self, Level* lvl, int x, int y, int z, int type);

    // Per face visibility test and per face vertex emission, used by the
    // shared render() to draw the 6 faces of a shaped tile. Liquids override
    // both to render only in the dedicated liquid layer and draw both sides.
    int  (*shouldRenderFace)(const Tile* self, const Level* lvl, int x, int y, int z, int layer, int face);
    void (*renderFace)(const Tile* self, Tessellator* t, int x, int y, int z, int face);
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
extern Tile TILE_BEDROCK;   // id=7, new in c0.0.14a_08, plain tile, unreachable from the hotbar

extern Tile TILE_WATER;      // id=8
extern Tile TILE_CALM_WATER; // id=9
extern Tile TILE_LAVA;       // id=10
extern Tile TILE_CALM_LAVA;  // id=11

extern Tile TILE_SAND;   // id=12, new in c0.0.14a_08, falls through air
extern Tile TILE_GRAVEL; // id=13, new in c0.0.14a_08, falls through air
extern Tile TILE_GOLD_ORE;  // id=14, new in c0.0.14a_08
extern Tile TILE_IRON_ORE;  // id=15, new in c0.0.14a_08
extern Tile TILE_COAL_ORE;  // id=16, new in c0.0.14a_08
extern Tile TILE_LOG;    // id=17, new in c0.0.14a_08, per face texture
extern Tile TILE_LEAVES; // id=18, new in c0.0.14a_08, non solid, non light blocking

void Tile_registerAll(void);

// Helper to render a plain, untextured face (for highlights)
void Face_render(Tessellator* t, int x, int y, int z, int face);

void Tile_onDestroy(const Tile* self, Level* lvl, int x, int y, int z, ParticleEngine* engine);

#endif