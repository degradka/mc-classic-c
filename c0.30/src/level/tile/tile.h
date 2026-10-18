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
    // an explosion only breaks tiles that have it
    bool explodable;

    // the shape, a full cube by default
    float xx0, yy0, zz0, xx1, yy1, zz1;

    int  (*getTexture)(const Tile* self, int face);
    // Tile.render, draws itself into a chunk list, true when it drew anything
    bool (*render)(const Tile* self, const Level* lvl, int x, int y, int z, Tessellator* t);
    // Tile.getRenderLayer, the chunk list it goes into, water 1, everything else 0
    int renderLayer;
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
    // after an explosion broke it, only tnt has it
    void (*onExploded)(const Tile* self, Level* lvl, int x, int y, int z);
    // only called right after the player places the tile
    void (*onBlockAdded)(const Tile* self, Level* lvl, int x, int y, int z);

    int  (*shouldRenderFace)(const Tile* self, const Level* lvl, int x, int y, int z, int face);
    void (*renderFace)(const Tile* self, Tessellator* t, int x, int y, int z, int face);

    // the held block in first person
    void (*renderItem)(const Tile* self, Tessellator* t);
    // the hotbar and block select icon, shaded per face, no begin/end
    void (*renderGuiItem)(const Tile* self, Tessellator* t);

    // Tile.quantityDropped, how many drop rolls Tile_dropItems makes
    int  (*getDropCount)(const Tile* self);
};

// indexed by tile id
extern const Tile* gTiles[256];

extern Tile TILE_ROCK;       // id=1
extern Tile TILE_GRASS;      // id=2
extern Tile TILE_DIRT;       // id=3
extern Tile TILE_STONEBRICK; // id=4
extern Tile TILE_WOOD;       // id=5
extern Tile TILE_BUSH;       // id=6
extern Tile TILE_BEDROCK;    // id=7

extern Tile TILE_WATER;      // id=8
extern Tile TILE_CALM_WATER; // id=9
extern Tile TILE_LAVA;       // id=10
extern Tile TILE_CALM_LAVA;  // id=11

extern Tile TILE_SAND;     // id=12
extern Tile TILE_GRAVEL;   // id=13
extern Tile TILE_GOLD_ORE; // id=14
extern Tile TILE_IRON_ORE; // id=15
extern Tile TILE_COAL_ORE; // id=16
extern Tile TILE_LOG;      // id=17
extern Tile TILE_LEAVES;   // id=18

extern Tile TILE_SPONGE; // id=19
extern Tile TILE_GLASS;  // id=20

// ids 21-36
extern Tile TILE_CLOTH[16];

extern Tile TILE_DANDELION;      // id=37, tex=13
extern Tile TILE_ROSE;           // id=38, tex=12
extern Tile TILE_MUSHROOM_BROWN; // id=39, tex=29
extern Tile TILE_MUSHROOM_RED;   // id=40, tex=28

extern Tile TILE_GOLD_BLOCK;  // id=41
extern Tile TILE_IRON_BLOCK;  // id=42
extern Tile TILE_DOUBLE_SLAB; // id=43
extern Tile TILE_SLAB;        // id=44
extern Tile TILE_BRICK;       // id=45
extern Tile TILE_TNT;         // id=46
extern Tile TILE_BOOKSHELF;   // id=47
extern Tile TILE_MOSSY_STONE; // id=48
extern Tile TILE_OBSIDIAN;    // id=49

void Tile_registerAll(void);

void Tile_onDestroy(const Tile* self, Level* lvl, int x, int y, int z, ParticleEngine* engine);
void Tile_spawnHitParticle(const Tile* self, Level* lvl, int x, int y, int z, int face, ParticleEngine* engine);

// the drop randoms are still rolled but no Item spawns
void Tile_dropItems(const Tile* self, Level* lvl, int x, int y, int z);
void Tile_renderFaceTex(const Tile* self, Tessellator* t, int x, int y, int z, int face, int tex);
// each roll passes with that chance, explosions use 0.3
void Tile_dropItemsChance(const Tile* self, Level* lvl, int x, int y, int z, float chance);

#endif