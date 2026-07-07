// tile.h: tile registry and game logic only, no rendering (server has no GL)

#ifndef TILE_H
#define TILE_H

#include "../level.h"

typedef struct Tile Tile;

// tile.getLiquidType() constants
#define LIQUID_NONE  0
#define LIQUID_WATER 1
#define LIQUID_LAVA  2

struct Tile {
    int id;
    int textureId; // kept for parity with the client's registry, unused server side
    int liquidType; // LIQUID_NONE / LIQUID_WATER / LIQUID_LAVA

    // Liquid only instance data, zero for regular tiles. Flowing and calm
    // variant ids, and how many tiles a spread can travel before stopping.
    int tileId, calmTileId, spreadSpeed;

    // c0.0.16a_02: how many extra 5 tick drains a scheduled reaction waits
    // before firing, on top of the tick it was queued on. 0 for every tile
    // except lava, which waits 5 (25 ticks), letting it fall/spread visibly
    // slower than water even though both use the same tick queue now.
    int tickDelay;

    // Collision shape, in block local coordinates. Default is a full cube
    // (0,0,0) to (1,1,1). Liquids crop the top so the surface sits slightly
    // below a full block. Kept for AABB queries even without rendering.
    float xx0, yy0, zz0, xx1, yy1, zz1;

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

    // server1.6: fired once when this tile is placed/removed at (x,y,z),
    // added for Sponge's dry-a-5x5x5-area-of-water mechanic. NULL for every
    // other tile (no op)
    void (*onPlace)(const Tile* self, Level* lvl, int x, int y, int z);
    void (*onRemoved)(const Tile* self, Level* lvl, int x, int y, int z);
};

// Global registry, index by tile id (0..255)
extern const Tile* gTiles[256];

// Predefined tiles
extern Tile TILE_ROCK;      // id=1
extern Tile TILE_GRASS;     // id=2 (custom per face)
extern Tile TILE_DIRT;      // id=3
// id=4 (StoneBrick) is intentionally not registered: server1.6's real source
// drops it from the tile registry entirely, gTiles[4] stays NULL. Client
// side keeps it registered, just off the hotbar/whitelist. This is a
// server-only removal, not a whole-game one
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

extern Tile TILE_SPONGE; // id=19, new in server1.6
extern Tile TILE_GLASS;  // id=20, new in server1.6

void Tile_registerAll(void);

#endif