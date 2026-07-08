// level/tile/tile.h: the server's tiles, ids 0 to 41, logic only

#ifndef TILE_H
#define TILE_H

#include "../../jrandom.h"
#include <stdbool.h>

struct Level;

enum { LIQUID_NONE, LIQUID_WATER, LIQUID_LAVA };

typedef struct Tile Tile;

struct Tile {
    int id;
    bool blocksLight;
    bool solid;
    int liquid;
    // extra 5 tick rounds a scheduled tick waits
    int tickDelay;
    // liquids, the flowing and the calm id
    int flowing, calm;
    void (*tick)(const Tile* t, struct Level* level, int x, int y, int z, JRandom* random);
    void (*neighborChanged)(const Tile* t, struct Level* level, int x, int y, int z, int type);
    void (*onPlace)(const Tile* t, struct Level* level, int x, int y, int z);
    // when the level puts it in or takes it out, the sponge uses them
    void (*onAdded)(const Tile* t, struct Level* level, int x, int y, int z);
    void (*onRemoved)(const Tile* t, struct Level* level, int x, int y, int z);
};

enum {
    TILE_ROCK = 1, TILE_GRASS, TILE_DIRT, TILE_STONEBRICK, TILE_WOOD, TILE_BUSH, TILE_UNBREAKABLE,
    TILE_WATER, TILE_CALM_WATER, TILE_LAVA, TILE_CALM_LAVA, TILE_SAND, TILE_GRAVEL,
    TILE_GOLD_ORE, TILE_IRON_ORE, TILE_COAL_ORE, TILE_LOG, TILE_LEAVES, TILE_SPONGE, TILE_GLASS,
    TILE_CLOTH_FIRST, TILE_CLOTH_LAST = 36, TILE_DANDELION, TILE_ROSE, TILE_BROWN_MUSHROOM, TILE_RED_MUSHROOM, TILE_GOLD
};

// which tiles the random ticks reach
extern bool Tile_shouldTick[256];

void Tile_registerAll(void);
// Tile.tiles[id], a negative id throws, the result can be NULL
const Tile* Tile_get(int id);
// the same but a NULL one throws a NullPointerException
const Tile* Tile_req(int id);

#endif
