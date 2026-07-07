// level/tile/tile.c

#include "tile.h"
#include "../level.h"
#include "../../jexc.h"
#include <string.h>

static Tile tiles[21];
static const Tile* registry[256];
bool Tile_shouldTick[256];

const Tile* Tile_get(int id) {
    if (id < 0 || id > 255) jthrowIndex(id);
    return registry[id];
}

const Tile* Tile_req(int id) {
    const Tile* t = Tile_get(id);
    if (!t) jthrowNull();
    return t;
}

static Tile* add(int id) {
    Tile* t = &tiles[id];
    memset(t, 0, sizeof *t);
    t->id = id;
    t->blocksLight = true;
    t->solid = true;
    registry[id] = t;
    return t;
}

static void Grass_tick(const Tile* t, Level* level, int x, int y, int z, JRandom* random) {
    (void)t;
    if (JRandom_nextIntN(random, 4) != 0) return;
    if (!Level_isLit(level, x, y + 1, z)) {
        Level_setTile(level, x, y, z, TILE_DIRT);
        return;
    }
    for (int i = 0; i < 4; ++i) {
        int nx = x + JRandom_nextIntN(random, 3) - 1;
        int ny = y + JRandom_nextIntN(random, 5) - 3;
        int nz = z + JRandom_nextIntN(random, 3) - 1;
        if (Level_getTile(level, nx, ny, nz) != TILE_DIRT || !Level_isLit(level, nx, ny + 1, nz)) continue;
        Level_setTile(level, nx, ny, nz, TILE_GRASS);
    }
}

static void Bush_tick(const Tile* t, Level* level, int x, int y, int z, JRandom* random) {
    (void)t; (void)random;
    int below = Level_getTile(level, x, y - 1, z);
    if (!Level_isLit(level, x, y, z) || (below != TILE_DIRT && below != TILE_GRASS)) {
        Level_setTile(level, x, y, z, 0);
    }
}

// sand and gravel only fall through air on this server
static void Falling_fall(Level* level, int x, int y, int z) {
    int ny = y;
    while (Level_getTile(level, x, ny - 1, z) == 0 && ny > 0) ny--;
    if (ny != y) Level_swap(level, x, y, z, x, ny, z);
}

static void Falling_onPlace(const Tile* t, Level* level, int x, int y, int z) {
    (void)t;
    Falling_fall(level, x, y, z);
}

static void Falling_neighborChanged(const Tile* t, Level* level, int x, int y, int z, int type) {
    (void)t; (void)type;
    Falling_fall(level, x, y, z);
}

static void Liquid_onPlace(const Tile* t, Level* level, int x, int y, int z) {
    Level_addToTickNextTick(level, x, y, z, t->flowing);
}

// water can't come within 2 blocks of a sponge
static bool Liquid_canFlow(const Tile* t, Level* level, int x, int y, int z) {
    if (t->liquid != LIQUID_WATER) return true;
    for (int i = x - 2; i <= x + 2; ++i)
    for (int j = y - 2; j <= y + 2; ++j)
    for (int k = z - 2; k <= z + 2; ++k) {
        if (Level_getTile(level, i, j, k) == TILE_SPONGE) return false;
    }
    return true;
}

// always false, so a liquid that spread sideways still goes calm
static bool Liquid_spread(const Tile* t, Level* level, int x, int y, int z) {
    if (Level_getTile(level, x, y, z) != 0) return false;
    if (!Liquid_canFlow(t, level, x, y, z)) return false;
    if (Level_setTile(level, x, y, z, t->flowing)) Level_addToTickNextTick(level, x, y, z, t->flowing);
    return false;
}

static void Liquid_tick(const Tile* t, Level* level, int x, int y, int z, JRandom* random) {
    (void)random;
    bool changed = false;
    // water keeps falling while it can be placed, lava stops after one
    while (Level_getTile(level, x, --y, z) == 0 && Liquid_canFlow(t, level, x, y, z)) {
        bool placed = Level_setTile(level, x, y, z, t->flowing);
        if (placed) changed = true;
        if (!placed || t->liquid == LIQUID_LAVA) break;
    }
    ++y;
    if (t->liquid == LIQUID_WATER || !changed) {
        changed |= Liquid_spread(t, level, x - 1, y, z);
        changed |= Liquid_spread(t, level, x + 1, y, z);
        changed |= Liquid_spread(t, level, x, y, z - 1);
        changed |= Liquid_spread(t, level, x, y, z + 1);
    }
    if (!changed) Level_setTileNoUpdate(level, x, y, z, t->calm);
    else Level_addToTickNextTick(level, x, y, z, t->flowing);
}

static bool mixes(const Tile* t, int type) {
    int other = Tile_req(type)->liquid;
    return (t->liquid == LIQUID_WATER && other == LIQUID_LAVA) || (other == LIQUID_WATER && t->liquid == LIQUID_LAVA);
}

static void Liquid_neighborChanged(const Tile* t, Level* level, int x, int y, int z, int type) {
    if (type != 0 && mixes(t, type)) {
        Level_setTile(level, x, y, z, TILE_ROCK);
        return;
    }
    // the neighbor's id, not this liquid's
    Level_addToTickNextTick(level, x, y, z, type);
}

static void CalmLiquid_neighborChanged(const Tile* t, Level* level, int x, int y, int z, int type) {
    bool air = false;
    if (Level_getTile(level, x - 1, y, z) == 0) air = true;
    if (Level_getTile(level, x + 1, y, z) == 0) air = true;
    if (Level_getTile(level, x, y, z - 1) == 0) air = true;
    if (Level_getTile(level, x, y, z + 1) == 0) air = true;
    if (Level_getTile(level, x, y - 1, z) == 0) air = true;
    if (type != 0 && mixes(t, type)) {
        Level_setTile(level, x, y, z, TILE_ROCK);
        return;
    }
    if (air) {
        Level_setTileNoUpdate(level, x, y, z, t->flowing);
        Level_addToTickNextTick(level, x, y, z, t->flowing);
    }
}

static void liquid(int id, int type, bool calm) {
    Tile* t = add(id);
    t->solid = false;
    t->liquid = type;
    t->tickDelay = type == LIQUID_LAVA ? 5 : 0;
    t->flowing = calm ? id - 1 : id;
    t->calm = calm ? id : id + 1;
    t->onPlace = Liquid_onPlace;
    if (calm) {
        t->neighborChanged = CalmLiquid_neighborChanged;
    } else {
        t->tick = Liquid_tick;
        t->neighborChanged = Liquid_neighborChanged;
        Tile_shouldTick[id] = true;
    }
}

// soaks up the water around it, and lets it flow back in when it's gone
static void Sponge_onAdded(const Tile* t, Level* level, int x, int y, int z) {
    (void)t;
    for (int i = x - 2; i <= x + 2; ++i)
    for (int j = y - 2; j <= y + 2; ++j)
    for (int k = z - 2; k <= z + 2; ++k) {
        if (Level_isWater(level, i, j, k)) Level_setTileNoNeighborChange(level, i, j, k, 0);
    }
}

static void Sponge_onRemoved(const Tile* t, Level* level, int x, int y, int z) {
    (void)t;
    for (int i = x - 2; i <= x + 2; ++i)
    for (int j = y - 2; j <= y + 2; ++j)
    for (int k = z - 2; k <= z + 2; ++k) {
        Level_updateNeighborsAt(level, i, j, k, Level_getTile(level, i, j, k));
    }
}

void Tile_registerAll(void) {
    memset(registry, 0, sizeof registry);
    memset(Tile_shouldTick, 0, sizeof Tile_shouldTick);
    add(TILE_ROCK);
    add(TILE_GRASS)->tick = Grass_tick;
    Tile_shouldTick[TILE_GRASS] = true;
    add(TILE_DIRT);
    add(TILE_STONEBRICK);
    add(TILE_WOOD);
    Tile* bush = add(TILE_BUSH);
    bush->blocksLight = false;
    bush->solid = false;
    bush->tick = Bush_tick;
    Tile_shouldTick[TILE_BUSH] = true;
    add(TILE_UNBREAKABLE);
    liquid(TILE_WATER, LIQUID_WATER, false);
    liquid(TILE_CALM_WATER, LIQUID_WATER, true);
    liquid(TILE_LAVA, LIQUID_LAVA, false);
    liquid(TILE_CALM_LAVA, LIQUID_LAVA, true);
    for (int id = TILE_SAND; id <= TILE_GRAVEL; ++id) {
        Tile* t = add(id);
        t->onPlace = Falling_onPlace;
        t->neighborChanged = Falling_neighborChanged;
    }
    add(TILE_GOLD_ORE);
    add(TILE_IRON_ORE);
    add(TILE_COAL_ORE);
    add(TILE_LOG);
    Tile* leaves = add(TILE_LEAVES);
    leaves->blocksLight = false;
    leaves->solid = false;
    Tile* sponge = add(TILE_SPONGE);
    sponge->onAdded = Sponge_onAdded;
    sponge->onRemoved = Sponge_onRemoved;
    Tile* glass = add(TILE_GLASS);
    glass->blocksLight = false;
    glass->solid = false;
}
