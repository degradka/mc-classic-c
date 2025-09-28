// level/tile/tile.h — tile registry, per-face textures, render helpers

#ifndef TILE_H
#define TILE_H

#include "../level.h"
#include "../../renderer/tessellator.h"
#include "../../player.h"

struct ParticleEngine; typedef struct ParticleEngine ParticleEngine;
typedef struct Tile Tile;
struct Player; typedef struct Player Player;

enum {
    LIQ_NONE  = 0,
    LIQ_WATER = 1,
    LIQ_LAVA  = 2
};

struct Tile {
    int id;
    int textureId;

    float x0, y0, z0, x1, y1, z1;

    int  (*getTexture)(const Tile* self, int face);
    void (*render)(const Tile* self, Tessellator* t, const Level* lvl, int layer, int x, int y, int z);
    void (*onTick)(const Tile* self, Level* lvl, int x, int y, int z);
    void (*neighborChanged)(const Tile* self, Level* lvl, int x, int y, int z, int changedType);

    int  (*isSolid)(const Tile* self);
    int  (*blocksLight)(const Tile* self);
    int  (*getAABB)(const Tile* self, int x, int y, int z, AABB* out);

    int  (*getLiquidType)(const Tile* self);
};

// Global registry, index by tile id (0..255)
extern const Tile* gTiles[256];

struct LiquidTile; typedef struct LiquidTile LiquidTile;

// Predefined tiles
extern Tile       TILE_ROCK;      // id=1
extern Tile       TILE_GRASS;     // id=2 (custom per face)
extern Tile       TILE_DIRT;      // id=3
extern Tile       TILE_STONEBRICK;// id=4
extern Tile       TILE_WOOD;      // id=5
extern Tile       TILE_BUSH;      // id=6
extern Tile       TILE_UNBREAKABLE; // id=7
extern LiquidTile TILE_WATER;       // id=8
extern LiquidTile TILE_CALM_WATER;  // id=9
extern LiquidTile TILE_LAVA;        // id=10
extern LiquidTile TILE_CALM_LAVA;   // id=11

void Tile_registerAll(void);

// Helper to render a plain, untextured face (for highlights)
void Face_render(Tessellator* t, int x, int y, int z, int face);
void Face_render_culled(const Player* p, Tessellator* t, int x, int y, int z, int face);

void Tile_setShape(Tile* t, float x0,float y0,float z0,float x1,float y1,float z1);

void Tile_onDestroy(const Tile* self, Level* lvl, int x, int y, int z, ParticleEngine* engine);

#endif