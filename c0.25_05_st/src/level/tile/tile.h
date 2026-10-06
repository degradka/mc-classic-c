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

// Tile$SoundType
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

    // liquids only, the flowing and calm ids
    int tileId, calmTileId, spreadSpeed;

    // extra 5 tick rounds a scheduled tick waits, only lava has it
    int tickDelay;

    // break particle gravity
    float particleGravity;

    // ticks of mining to break it, 0 breaks instantly
    int hardnessTicks;

    // Tile.tickOnLoad, picked by the level's random tile updates
    bool ticksRandomly;

    // Tile.isCube, drawn face by face by the chunk builder. False for liquids and plants
    bool isCube;

    // the shape, a full cube by default
    float xx0, yy0, zz0, xx1, yy1, zz1;

    int  (*getTexture)(const Tile* self, int face);
    void (*render)(const Tile* self, Tessellator* t, const Level* lvl, int layer, int x, int y, int z);
    void (*onTick)(const Tile* self, Level* lvl, int x, int y, int z);

    int  (*isSolid)(const Tile* self);
    int  (*blocksLight)(const Tile* self);
    // 0 when there is no collision box
    int  (*getAABB)(const Tile* self, int x, int y, int z, AABB* out);
    // false for liquids, you can't aim at them
    int  (*mayPick)(const Tile* self);

    void (*neighborChanged)(const Tile* self, Level* lvl, int x, int y, int z, int type);

    // only the sponge has these
    void (*onPlace)(const Tile* self, Level* lvl, int x, int y, int z);
    void (*onRemoved)(const Tile* self, Level* lvl, int x, int y, int z);
    // only called right after the player places the tile
    void (*onBlockAdded)(const Tile* self, Level* lvl, int x, int y, int z);

    int  (*shouldRenderFace)(const Tile* self, const Level* lvl, int x, int y, int z, int layer, int face);
    void (*renderFace)(const Tile* self, Tessellator* t, int x, int y, int z, int face);

    // the held block in first person
    void (*renderItem)(const Tile* self, Tessellator* t);

    // how many items break off and which tile they are, one of itself by default
    int  (*getDropCount)(const Tile* self);
    int  (*getDropResource)(const Tile* self);
};

// indexed by tile id
extern const Tile* gTiles[256];

extern Tile TILE_ROCK;      // id=1
extern Tile TILE_GRASS;     // id=2
extern Tile TILE_DIRT;      // id=3
extern Tile TILE_STONEBRICK;// id=4
extern Tile TILE_WOOD;      // id=5
extern Tile TILE_BUSH;      // id=6
extern Tile TILE_BEDROCK;   // id=7

extern Tile TILE_WATER;      // id=8
extern Tile TILE_CALM_WATER; // id=9
extern Tile TILE_LAVA;       // id=10
extern Tile TILE_CALM_LAVA;  // id=11

extern Tile TILE_SAND;   // id=12
extern Tile TILE_GRAVEL; // id=13
extern Tile TILE_GOLD_ORE;  // id=14
extern Tile TILE_IRON_ORE;  // id=15
extern Tile TILE_COAL_ORE;  // id=16
extern Tile TILE_LOG;    // id=17
extern Tile TILE_LEAVES; // id=18

extern Tile TILE_SPONGE; // id=19
extern Tile TILE_GLASS;  // id=20

// ids 21-36
extern Tile TILE_CLOTH[16];

extern Tile TILE_DANDELION;      // id=37, tex=13
extern Tile TILE_ROSE;           // id=38, tex=12
extern Tile TILE_MUSHROOM_BROWN; // id=39, tex=29
extern Tile TILE_MUSHROOM_RED;   // id=40, tex=28

extern Tile TILE_GOLD_BLOCK; // id=41, tex=40, plain tile, no special behavior

void Tile_registerAll(void);

void Tile_onDestroy(const Tile* self, Level* lvl, int x, int y, int z, ParticleEngine* engine);
void Tile_spawnHitParticle(const Tile* self, Level* lvl, int x, int y, int z, int face, ParticleEngine* engine);

// spawns the drops, each at a random point inside the broken block
void Tile_dropItems(const Tile* self, Level* lvl, int x, int y, int z);
// the chunk builder's face, grown by 0.001 (z max shrinks)
void Tile_renderCubeFace(const Tile* self, Tessellator* t, int x, int y, int z, int face);
void Tile_renderFaceTex(const Tile* self, Tessellator* t, int x, int y, int z, int face, int tex);
// each item only drops with that chance, explosions use 0.3
void Tile_dropItemsChance(const Tile* self, Level* lvl, int x, int y, int z, float chance);

#endif