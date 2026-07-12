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

// c0.0.23a_01: Tile$SoundType, gates footstep/break sounds. Real source pairs
// each with a base volume/pitch, both further randomized per play (see
// SoundType_getVolume/getPitch)
typedef enum {
    SOUND_NONE = 0,
    SOUND_GRASS,
    SOUND_CLOTH,
    SOUND_GRAVEL,
    SOUND_STONE,
    SOUND_METAL,
    SOUND_WOOD,
    SOUND_TYPE_COUNT
} SoundType;

typedef struct {
    const char* name; // "step.<name>" is the sound lookup key
    float volume;
    float pitch;
} SoundTypeDef;

extern const SoundTypeDef SOUND_TYPES[SOUND_TYPE_COUNT];

float SoundType_getVolume(SoundType t);
float SoundType_getPitch(SoundType t);

struct Tile {
    int id;
    int textureId;
    int liquidType; // LIQUID_NONE / LIQUID_WATER / LIQUID_LAVA
    SoundType soundType;

    // Liquid only instance data, zero for regular tiles. Flowing and calm
    // variant ids, and how many tiles a spread can travel before stopping.
    int tileId, calmTileId, spreadSpeed;

    // c0.0.16a_02: how many extra 5 tick drains a scheduled reaction waits
    // before firing, on top of the tick it was queued on. 0 for every tile
    // except lava, which waits 5 (25 ticks), letting it fall/spread visibly
    // slower than water even though both use the same tick queue now.
    int tickDelay;

    // c0.0.16a_02: scales a destroyed tile's particle gravity (-0.04/tick
    // base). 1.0 for every tile except Leaves, which falls at 0.4x.
    float particleGravity;

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

    // c0.0.19a_04: fired once when this tile is placed/removed at (x,y,z),
    // added for Sponge's dry-a-5x5x5-area-of-water mechanic. NULL for every
    // other tile (no op)
    void (*onPlace)(const Tile* self, Level* lvl, int x, int y, int z);
    void (*onRemoved)(const Tile* self, Level* lvl, int x, int y, int z);

    // Per face visibility test and per face vertex emission, used by the
    // shared render() to draw the 6 faces of a shaped tile. Liquids override
    // both to render only in the dedicated liquid layer and draw both sides.
    int  (*shouldRenderFace)(const Tile* self, const Level* lvl, int x, int y, int z, int layer, int face);
    void (*renderFace)(const Tile* self, Tessellator* t, int x, int y, int z, int face);

    // c0.24_st_03: how many Item entities Tile_dropItems spawns on break
    // (default 1) and which tile id they represent (default this tile's own
    // id). Matches the real source's Tile.f()/g(). Per-tile overrides (Log's
    // 3-5 planks, Leaves' 1-in-6 sapling, etc) are task #63's job; every tile
    // gets the default here for now, i.e. breaking anything currently drops
    // exactly one of itself
    int  (*getDropCount)(const Tile* self);
    int  (*getDropResource)(const Tile* self);
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

extern Tile TILE_SPONGE; // id=19, new in c0.0.19a_04, dries a 5x5x5 area of water on placement
extern Tile TILE_GLASS;  // id=20, new in c0.0.19a_04, non light blocking, doesn't double-face with neighboring glass

// c0.0.20a_02: 16 Cloth colors, ids 21..36, textures 64..79 (one full terrain.png
// row). All plain tiles, no special behavior beyond the texture id, placeable
// only (no world generation spawn)
extern Tile TILE_CLOTH[16];

// c0.0.20a_02: reuse Bush's tile class (isSolid/blocksLight/getAABB/render/onTick),
// placeable only, no world generation spawn
extern Tile TILE_DANDELION;      // id=37, tex=13
extern Tile TILE_ROSE;           // id=38, tex=12
extern Tile TILE_MUSHROOM_BROWN; // id=39, tex=29
extern Tile TILE_MUSHROOM_RED;   // id=40, tex=28

extern Tile TILE_GOLD_BLOCK; // id=41, tex=40, plain tile, no special behavior

void Tile_registerAll(void);

// Helper to render a plain, untextured face (for highlights)
void Face_render(Tessellator* t, int x, int y, int z, int face, float px, float py, float pz);

void Tile_onDestroy(const Tile* self, Level* lvl, int x, int y, int z, ParticleEngine* engine);

// c0.24_st_03: spawns self->getDropCount() Item entities of id
// self->getDropResource(), each jittered to a random point within the
// broken block (matches Tile.e()/Tile.a(level,x,y,z,1.0f) exactly)
void Tile_dropItems(const Tile* self, Level* lvl, int x, int y, int z);

#endif