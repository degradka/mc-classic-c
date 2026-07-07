// tile.c: registry and game logic only, no rendering (server has no GL)

#include "tile.h"
#include <string.h>

static int Tile_default_isSolid(const Tile* self)    { (void)self; return 1; }
static int Tile_default_blocksLight(const Tile* self){ (void)self; return 1; }
static int Tile_default_mayPick(const Tile* self)    { (void)self; return 1; }
static int Tile_default_getAABB(const Tile* self, int x,int y,int z, AABB* out) {
    (void)self;
    if (out) *out = AABB_create(x, y, z, x+1, y+1, z+1);
    return 1; // has AABB
}

/* tile instances and registry */

const Tile* gTiles[256] = { 0 };

static void Tile_default_neighborChanged(const Tile* self, Level* lvl, int x, int y, int z, int type) {
    (void)self; (void)lvl; (void)x; (void)y; (void)z; (void)type;
}

static void registerTile(Tile* t, int id, int tex) {
    t->id = id; t->textureId = tex;
    t->liquidType = LIQUID_NONE;
    t->tileId = t->calmTileId = t->spreadSpeed = 0;
    t->tickDelay = 0;
    t->xx0 = t->yy0 = t->zz0 = 0.0f;
    t->xx1 = t->yy1 = t->zz1 = 1.0f;
    t->onTick = NULL;
    t->isSolid     = Tile_default_isSolid;
    t->blocksLight = Tile_default_blocksLight;
    t->getAABB     = Tile_default_getAABB;
    t->mayPick     = Tile_default_mayPick;
    t->neighborChanged = Tile_default_neighborChanged;

    gTiles[id] = t;
}

Tile TILE_ROCK;
Tile TILE_DIRT;
Tile TILE_STONEBRICK;
Tile TILE_WOOD;
Tile TILE_GRASS;
Tile TILE_BUSH;
Tile TILE_BEDROCK;
Tile TILE_WATER;
Tile TILE_CALM_WATER;
Tile TILE_LAVA;
Tile TILE_CALM_LAVA;
Tile TILE_SAND;
Tile TILE_GRAVEL;
Tile TILE_GOLD_ORE;
Tile TILE_IRON_ORE;
Tile TILE_COAL_ORE;
Tile TILE_LOG;
Tile TILE_LEAVES;

static void Grass_onTick(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    if (rand() % 4 != 0) return; // c0.0.13a throttles grass ticks to 25%

    if (!Level_isLit(lvl, x, y + 1, z)) {
        // no sunlight reaching the block above: turn into dirt
        level_setTile(lvl, x, y, z, TILE_DIRT.id);
    } else {
        // try 4 random neighbors, matching Java's skewed vertical range
        for (int i = 0; i < 4; ++i) {
            int tx = x + (rand() % 3) - 1;
            int ty = y + (rand() % 5) - 3;
            int tz = z + (rand() % 3) - 1;
            if (Level_getTile(lvl, tx, ty, tz) == TILE_DIRT.id && Level_isLit(lvl, tx, ty + 1, tz)) {
                level_setTile(lvl, tx, ty, tz, TILE_GRASS.id);
            }
        }
    }
}

static int Bush_isSolid(const Tile* self)     { (void)self; return 0; }
static int Bush_blocksLight(const Tile* self) { (void)self; return 0; }
static int Bush_getAABB(const Tile* self, int x,int y,int z, AABB* out){
    (void)self; (void)x; (void)y; (void)z; (void)out;
    return 0; // no collision box
}

/* Liquids, see the tile.h comment above the externs */
static int Liquid_isSolid(const Tile* self) { (void)self; return 0; }
static int Liquid_getAABB(const Tile* self, int x,int y,int z, AABB* out){
    (void)self; (void)x; (void)y; (void)z; (void)out;
    return 0; // no collision box, matches LiquidTile.getAABB() returning null
}
static int Liquid_mayPick(const Tile* self) { (void)self; return 0; }

// Spreads one cell into an air neighbor and schedules its own next tick,
// used by the falling/spreading pass below. Always returns nothing useful by
// design (matches the real source, which discards this helper's result and
// bases the calm/flowing decision purely on the fall loop below), the point
// is the side effect of placing a tile and scheduling its next tick.
static void Liquid_spreadToNeighbor(const Tile* self, Level* lvl, int x, int y, int z) {
    if (Level_getTile(lvl, x, y, z) == 0 && level_setTile(lvl, x, y, z, self->tileId)) {
        Level_addToTickNextTick(lvl, x, y, z, self->tileId);
    }
}

// c0.0.14a_08 replaced the old spreadSpeed capped recursive burst with the
// new pending tick queue: falls straight down through air in one go same as
// before, then spreads to the 4 side neighbors (scheduling each newly placed
// neighbor's own next tick), and only freezes into the calm variant if the
// fall loop didn't move anything this tick, otherwise keeps rescheduling
// itself. This changes lava's relative flow speed since there's no more
// explicit per-tick spread distance cap, it's now governed by tick queue
// timing (drained every 5 ticks) rather than a fixed recursion depth.
static void Liquid_tick(const Tile* self, Level* lvl, int x, int y, int z) {
    int hasChanged = 0;

    // y > 0 guard stops the fall at the map floor. Level_getTile reads out
    // of bounds y as air, so without this an open shaft reaching y 0 makes
    // the loop fall forever and hangs the game.
    while (y > 0 && Level_getTile(lvl, x, y - 1, z) == 0) {
        y--;
        if (level_setTile(lvl, x, y, z, self->tileId)) hasChanged = 1;
    }

    if (self->liquidType == LIQUID_WATER || !hasChanged) {
        Liquid_spreadToNeighbor(self, lvl, x - 1, y, z);
        Liquid_spreadToNeighbor(self, lvl, x + 1, y, z);
        Liquid_spreadToNeighbor(self, lvl, x, y, z - 1);
        Liquid_spreadToNeighbor(self, lvl, x, y, z + 1);
    }

    if (!hasChanged) {
        Level_setTileNoUpdate(lvl, x, y, z, self->calmTileId);
    } else {
        Level_addToTickNextTick(lvl, x, y, z, self->tileId);
    }
}

static void Liquid_neighborChanged(const Tile* self, Level* lvl, int x, int y, int z, int type) {
    if (self->liquidType == LIQUID_WATER && (type == TILE_LAVA.id || type == TILE_CALM_LAVA.id)) {
        Level_setTileNoUpdate(lvl, x, y, z, TILE_ROCK.id);
    }
    if (self->liquidType == LIQUID_LAVA && (type == TILE_WATER.id || type == TILE_CALM_WATER.id)) {
        Level_setTileNoUpdate(lvl, x, y, z, TILE_ROCK.id);
    }
}

static void CalmLiquid_neighborChanged(const Tile* self, Level* lvl, int x, int y, int z, int type) {
    int hasAirNeighbor = 0;
    if (Level_getTile(lvl, x - 1, y, z) == 0) hasAirNeighbor = 1;
    if (Level_getTile(lvl, x + 1, y, z) == 0) hasAirNeighbor = 1;
    if (Level_getTile(lvl, x, y, z - 1) == 0) hasAirNeighbor = 1;
    if (Level_getTile(lvl, x, y, z + 1) == 0) hasAirNeighbor = 1;
    if (Level_getTile(lvl, x, y - 1, z) == 0) hasAirNeighbor = 1;

    // c0.0.14a_08: the rock conversion check now returns immediately, and
    // restarting flow now also schedules a tick right away instead of
    // waiting for an ambient random tick to notice the tile is flowing again
    if (self->liquidType == LIQUID_WATER && type == TILE_LAVA.id) {
        Level_setTileNoUpdate(lvl, x, y, z, TILE_ROCK.id);
        return;
    }
    if (self->liquidType == LIQUID_LAVA && type == TILE_WATER.id) {
        Level_setTileNoUpdate(lvl, x, y, z, TILE_ROCK.id);
        return;
    }
    if (hasAirNeighbor) {
        Level_setTileNoUpdate(lvl, x, y, z, self->tileId); // start flowing again
        Level_addToTickNextTick(lvl, x, y, z, self->tileId);
    }
}

static void Bush_onTick(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    int below = Level_getTile(lvl, x, y-1, z);
    if (!Level_isLit(lvl, x, y, z) || (below != TILE_DIRT.id && below != TILE_GRASS.id)) {
        level_setTile(lvl, x, y, z, 0);
    }
}

/* Sand/Gravel: new in c0.0.14a_08, falls straight down through air on both
   a random tick and instantly on any neighbor change */
static void FallingTile_fall(Level* lvl, int x, int y, int z) {
    int j = y;
    while (Level_getTile(lvl, x, j - 1, z) == 0 && j > 0) j--;
    if (j != y) Level_swap(lvl, x, y, z, x, j, z);
}
static void FallingTile_onTick(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    FallingTile_fall(lvl, x, y, z);
}
static void FallingTile_neighborChanged(const Tile* self, Level* lvl, int x, int y, int z, int type) {
    (void)self; (void)type;
    FallingTile_fall(lvl, x, y, z);
}

/* Leaves: new in c0.0.14a_08, non solid, non light blocking, planted by
   world generation's new tree pass */
static int Leaves_isSolid(const Tile* self)     { (void)self; return 0; }
static int Leaves_blocksLight(const Tile* self) { (void)self; return 0; }

void Tile_registerAll(void) {
    memset((void*)gTiles, 0, sizeof(gTiles));
    registerTile(&TILE_ROCK,       1,  1);
    registerTile(&TILE_GRASS,      2,  3);
    registerTile(&TILE_DIRT,       3,  2);
    registerTile(&TILE_STONEBRICK, 4, 16);
    registerTile(&TILE_WOOD,       5,  4);
    registerTile(&TILE_BUSH,       6, 15);
    registerTile(&TILE_BEDROCK,    7, 17);

    TILE_GRASS.onTick     = Grass_onTick;

    TILE_BUSH.isSolid     = Bush_isSolid;
    TILE_BUSH.blocksLight = Bush_blocksLight;
    TILE_BUSH.getAABB     = Bush_getAABB;
    TILE_BUSH.onTick      = Bush_onTick;

    // tex 14 = water, tex 30 = lava (terrain.png atlas slots, matching LiquidTile.java)
    registerTile(&TILE_WATER,      8, 14);
    registerTile(&TILE_CALM_WATER, 9, 14);
    registerTile(&TILE_LAVA,      10, 30);
    registerTile(&TILE_CALM_LAVA, 11, 30);

    TILE_WATER.liquidType      = LIQUID_WATER;
    TILE_CALM_WATER.liquidType = LIQUID_WATER;
    TILE_LAVA.liquidType       = LIQUID_LAVA;
    TILE_CALM_LAVA.liquidType  = LIQUID_LAVA;

    TILE_WATER.isSolid      = Liquid_isSolid;
    TILE_CALM_WATER.isSolid = Liquid_isSolid;
    TILE_LAVA.isSolid       = Liquid_isSolid;
    TILE_CALM_LAVA.isSolid  = Liquid_isSolid;

    TILE_WATER.getAABB      = Liquid_getAABB;
    TILE_CALM_WATER.getAABB = Liquid_getAABB;
    TILE_LAVA.getAABB       = Liquid_getAABB;
    TILE_CALM_LAVA.getAABB  = Liquid_getAABB;

    TILE_WATER.mayPick      = Liquid_mayPick;
    TILE_CALM_WATER.mayPick = Liquid_mayPick;
    TILE_LAVA.mayPick       = Liquid_mayPick;
    TILE_CALM_LAVA.mayPick  = Liquid_mayPick;

    // shape is a full block minus a thin sliver off the top, so the surface
    // sits slightly below a full block
    TILE_WATER.yy0 = TILE_CALM_WATER.yy0 = TILE_LAVA.yy0 = TILE_CALM_LAVA.yy0 = -0.1f;
    TILE_WATER.yy1 = TILE_CALM_WATER.yy1 = TILE_LAVA.yy1 = TILE_CALM_LAVA.yy1 = 0.9f;

    // flowing variants: tileId==own id, calmTileId==the paired calm id
    TILE_WATER.tileId = TILE_WATER.id; TILE_WATER.calmTileId = TILE_CALM_WATER.id; TILE_WATER.spreadSpeed = 8;
    TILE_LAVA.tileId  = TILE_LAVA.id;  TILE_LAVA.calmTileId  = TILE_CALM_LAVA.id;  TILE_LAVA.spreadSpeed  = 2;
    // calm variants share the same flowing/calm id pair as their flowing counterpart
    TILE_CALM_WATER.tileId = TILE_WATER.id; TILE_CALM_WATER.calmTileId = TILE_CALM_WATER.id; TILE_CALM_WATER.spreadSpeed = 8;
    TILE_CALM_LAVA.tileId  = TILE_LAVA.id;  TILE_CALM_LAVA.calmTileId  = TILE_CALM_LAVA.id;  TILE_CALM_LAVA.spreadSpeed  = 2;

    // c0.0.16a_02: lava's scheduled reactions wait 5 extra 5 tick drains (25
    // ticks) before firing, so it visibly lags behind water on the same
    // tick queue instead of resolving at the same rate
    TILE_LAVA.tickDelay      = 5;
    TILE_CALM_LAVA.tickDelay = 5;

    TILE_WATER.onTick = Liquid_tick;
    TILE_LAVA.onTick  = Liquid_tick;
    // calm variants don't tick (setTicking(false) in the original)

    TILE_WATER.neighborChanged      = Liquid_neighborChanged;
    TILE_LAVA.neighborChanged       = Liquid_neighborChanged;
    TILE_CALM_WATER.neighborChanged = CalmLiquid_neighborChanged;
    TILE_CALM_LAVA.neighborChanged  = CalmLiquid_neighborChanged;

    registerTile(&TILE_SAND,       12, 18);
    registerTile(&TILE_GRAVEL,     13, 19);
    registerTile(&TILE_GOLD_ORE,   14, 32);
    registerTile(&TILE_IRON_ORE,   15, 33);
    registerTile(&TILE_COAL_ORE,   16, 34);
    // server1.2's own tile id 17 is a plain single-texture tile, unlike the
    // client's per-face TILE_LOG, matching the real server, not a
    // copy-paste oversight
    registerTile(&TILE_LOG,        17,  0);
    registerTile(&TILE_LEAVES,     18, 22);

    TILE_SAND.onTick   = TILE_GRAVEL.onTick   = FallingTile_onTick;
    TILE_SAND.neighborChanged = TILE_GRAVEL.neighborChanged = FallingTile_neighborChanged;

    TILE_LEAVES.isSolid     = Leaves_isSolid;
    TILE_LEAVES.blocksLight = Leaves_blocksLight;
}
