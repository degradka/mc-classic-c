// level.h: world storage, lighting columns, IO, and solid cube queries

#ifndef LEVEL_H
#define LEVEL_H

#include "common.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "../phys/aabb.h"
#include <math.h>

typedef unsigned char byte;

struct LevelRenderer; typedef struct LevelRenderer LevelRenderer;
struct Entity; typedef struct Entity Entity;

// a pending liquid/gravity-tile reaction scheduled for a future tick, see
// Level_addToTickNextTick. delay is extra 5-tick drain cycles to wait before
// firing (c0.0.16a_02, lava only, see Tile.tickDelay).
typedef struct {
    int x, y, z;
    int tileId;
    int delay;
} TickEntry;

typedef struct Level {
    int   width, height, depth;
    byte* blocks;
    int*  lightDepths;
    LevelRenderer* renderer;  // backref used for dirty notifications
    int unprocessed;

    // save metadata, round tripped through level.dat but not used elsewhere
    char name[64];
    char creator[64];
    long long createTime;

    // c0.0.14a_08: a real spawn point instead of scattering entities randomly
    int   xSpawn, ySpawn, zSpawn;
    float rotSpawn;

    // c0.0.14a_08: the packed tick-random LCG state (Level.d in the real
    // source), separate from libc rand() used for everything else
    unsigned int tickRandom;
    int tickCount;

    TickEntry* tickList;
    int tickListSize;
    int tickListCapacity;

    // c0.0.19a_04: true for the lifetime of a level installed from a network
    // connection. While true, level_setTile is a no-op (server-authoritative
    // block mutation): only Level_netSetTile, driven by an incoming SetBlock
    // packet, actually changes tiles. Reset to false whenever a singleplayer
    // level is (re)generated
    bool networkMode;

    // c0.24_st_03: the local singleplayer/own Player, used by mob AI to find
    // a chase/attack target (real source's Level.player, set once from
    // Player's own constructor). NULL until Minecraft's own init sets it,
    // never reassigned afterward for the lifetime of the process
    Entity* player;

    // c0.27_st: true for the duration of Level_explode's own destroy loop.
    // TNT's real tile class has two genuinely distinct removal hooks:
    // direct mining spawns a primed entity with the normal fresh 40 tick
    // fuse, but being caught in another explosion's blast (Level.explode's
    // own f(Level,x,y,z) "wasExploded" hook, bytecode-confirmed via javap
    // since CFR's decompile of it looked internally inconsistent) spawns one
    // with a much shorter randomized fuse instead. Both paths route through
    // this port's single shared onRemoved hook (Tnt_onRemoved), so this flag
    // is how it tells which real hook it's standing in for
    bool inExplosion;
} Level;

typedef struct {
    int   size;
    int   capacity;
    AABB* aabbs;
} ArrayList_AABB;

ArrayList_AABB Level_getCubes(const Level* level, const AABB* boundingBox);

void  Level_init(Level* level, int width, int height, int depth);
void  Level_destroy(Level* level);
// frees the existing blocks and lightDepths, allocates at the new size, and
// regenerates. Caller must rebuild the LevelRenderer afterward since its
// chunk grid is sized for the old dimensions.
void  Level_resize(Level* level, int width, int height, int depth);
void  Level_setDataFromNetwork(Level* level, int width, int height, int depth, const byte* blocks);

bool  Level_isTile(const Level* level, int x, int y, int z);
bool  Level_isSolidTile(const Level* level, int x, int y, int z);
bool  Level_isLightBlocker(const Level* level, int x, int y, int z);

void  calcLightDepths(Level* level, int minX, int minZ, int maxX, int maxZ);

void  Level_generateMap(Level* level);

bool  Level_load(Level* level);
void  Level_save(const Level* level);

// no-op (returns false) while level->networkMode is true, matching the real
// source's setTile/setTileNoNeighborChange gating: in multiplayer, only the
// server's own echoed changes (via Level_netSetTile) are allowed to mutate
// the level, not local world-gen/player-initiated calls
bool  level_setTile(Level* level, int x, int y, int z, int type);
// the always-executing core level_setTile delegates to when not in network
// mode. Used directly by the incoming SetBlock packet handler, which must
// always be able to apply the server's authoritative change regardless of
// networkMode
bool  Level_netSetTile(Level* level, int x, int y, int z, int type);
bool  Level_setTileNoUpdate(Level* level, int x, int y, int z, int type);
int   Level_getTile(const Level* level, int x, int y, int z);

bool  Level_isLit(const Level* level, int x, int y, int z);
float Level_getBrightness(const Level* level, int x, int y, int z);

bool  Level_containsAnyLiquid(const Level* level, const AABB* box);
bool  Level_containsLiquid(const Level* level, const AABB* box, int liquidId);

void  Level_onTick(Level* level);

int   Level_getHighestTile(const Level* level, int x, int z);
float Level_getGroundLevel(const Level* level);
float Level_getWaterLevel(const Level* level);
// picks a spawn point on a column near the map center, above water level.
// Called automatically after Level_generateMap/Level_load.
void  Level_findSpawn(Level* level);
void  Level_setSpawnPos(Level* level, int x, int y, int z, float rot);

// swaps two tile positions with no neighbor notification of the source cell,
// used by the falling-block (Sand/Gravel) tile family
void  Level_swap(Level* level, int x1, int y1, int z1, int x2, int y2, int z2);

// schedules a tile reaction for a future tick (drained every 5 ticks), used
// by liquids and the falling-block tile family instead of resolving inline
void  Level_addToTickNextTick(Level* level, int x, int y, int z, int tileId);

// c0.0.19a_04: re-invokes the tile at (x,y,z)'s own neighborChanged callback,
// passing its own current tile id as the "type" argument, a way to force a
// cell to re-evaluate its reaction logic without an actual neighbor changing.
// Added for Sponge's onRemoved hook, which re-triggers this over a 5x5x5 area
void  Level_updateNeighborsAt(Level* level, int x, int y, int z);

// c0.24_st_03: mob AI's chase target. Set once, from Player_init's caller
static inline void   Level_setPlayer(Level* level, Entity* player) { level->player = player; }
static inline Entity* Level_getPlayer(const Level* level) { return level->player; }

// line of sight test between two points, used to gate mob melee attacks
// (matches Level.clip(Vec3,Vec3) != null): true if a solid tile blocks the
// straight line between them before reaching the second point
bool Level_clip(const Level* level, float x1, float y1, float z1, float x2, float y2, float z2);

// attempts to grow a tree with its base at (x,y,z), matches
// Level.maybeGrowTree() exactly: trunk 4 to 6 tall, a wider 5x5 canopy base
// narrowing to a 3x3 top with randomly dropped corners, forced off on the
// very top layer. Requires clear space for the whole shape and grass
// directly underneath, returns false with nothing placed if either fails,
// matching the real source's own check space first, place second order
bool Level_maybeGrowTree(Level* level, int x, int y, int z);

// matches Level.explode(Entity,x,y,z,radius): clears every block whose
// center falls within radius of (x,y,z), each destroyed block also getting
// a reduced chance of dropping its usual items, then damages every nearby
// entity scaled by how close it is to the center. source is credited as
// the attacker on any entity hurt (matches Creeper's own death explosion,
// character/creature.c's Creeper_onDeathTimeout)
void Level_explode(Level* level, Entity* source, float x, float y, float z, float radius);

#endif  // LEVEL_H