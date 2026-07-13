// level/tile/tile.h: tile registry, textures and render helpers

#ifndef TILE_H
#define TILE_H

#include "../level.h"
#include "../../renderer/tessellator.h"

struct ParticleEngine; typedef struct ParticleEngine ParticleEngine;

typedef struct Tile Tile;

// Tile.SoundType, cloth sounds like grass and metal like stone
enum { SOUND_NONE, SOUND_GRASS, SOUND_CLOTH, SOUND_GRAVEL, SOUND_STONE, SOUND_METAL, SOUND_WOOD };
typedef struct {
    const char* name;
    float volume, pitch;
} SoundType;
extern const SoundType SOUND_TYPES[7];
// both draw from the tiles' shared random
float SoundType_getVolume(const SoundType* st);
float SoundType_getPitch(const SoundType* st);

// Tile.getLiquidType
#define LIQUID_NONE  0
#define LIQUID_WATER 1
#define LIQUID_LAVA  2

struct Tile {
    int id;
    int textureId;
    int liquidType; // LIQUID_NONE / LIQUID_WATER / LIQUID_LAVA

    // liquids only, the flowing and calm ids
    int tileId, calmTileId;
    // picked by the level's random ticks
    bool shouldTick;
    // how much gravity the destroy particles get
    float gravity;
    int soundType;
    // extra tick list rounds before a scheduled tick runs, 5 for lava
    int tickDelay;

    // the shape, a full cube by default
    float xx0, yy0, zz0, xx1, yy1, zz1;

    int  (*getTexture)(const Tile* self, int face);
    // true when a face was drawn
    bool (*render)(const Tile* self, Tessellator* t, const Level* lvl, int layer, int x, int y, int z);
    // random ticks and the level's scheduled ticks, may be NULL
    void (*onTick)(const Tile* self, Level* lvl, int x, int y, int z);

    int  (*isSolid)(const Tile* self);
    int  (*blocksLight)(const Tile* self);
    // 0 when there is no collision box
    int  (*getAABB)(const Tile* self, int x, int y, int z, AABB* out);

    void (*neighborChanged)(const Tile* self, Level* lvl, int x, int y, int z, int type);
    // after the player places it
    void (*onPlace)(const Tile* self, Level* lvl, int x, int y, int z);
    // the level calls these when it puts the tile in or takes it out
    void (*onAdded)(const Tile* self, Level* lvl, int x, int y, int z);
    void (*onRemoved)(const Tile* self, Level* lvl, int x, int y, int z);

    int  (*shouldRenderFace)(const Tile* self, const Level* lvl, int x, int y, int z, int layer, int face);
    // the light a face gets from the tile in front of it
    float (*getBrightness)(const Tile* self, const Level* lvl, int x, int y, int z);
    void (*renderFace)(const Tile* self, Tessellator* t, int x, int y, int z, int face);

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
extern Tile TILE_UNBREAKABLE; // id=7

extern Tile TILE_WATER;      // id=8
extern Tile TILE_CALM_WATER; // id=9
extern Tile TILE_LAVA;       // id=10
extern Tile TILE_CALM_LAVA;  // id=11
extern Tile TILE_SAND;       // id=12
extern Tile TILE_GRAVEL;     // id=13
extern Tile TILE_GOLD_ORE;   // id=14
extern Tile TILE_IRON_ORE;   // id=15
extern Tile TILE_COAL_ORE;   // id=16
extern Tile TILE_LOG;        // id=17
extern Tile TILE_LEAVES;     // id=18
extern Tile TILE_SPONGE;     // id=19
extern Tile TILE_GLASS;      // id=20
// 21 to 36, the 16 cloth colors
extern Tile TILE_CLOTH[16];
extern Tile TILE_DANDELION;  // id=37
extern Tile TILE_ROSE;       // id=38
extern Tile TILE_BROWN_MUSHROOM; // id=39
extern Tile TILE_RED_MUSHROOM;   // id=40
extern Tile TILE_GOLD_BLOCK; // id=41

void Tile_registerAll(void);

// Helper to render a plain, untextured face (for highlights)
void Face_render(Tessellator* t, int x, int y, int z, int face, float px, float py, float pz);
// the tile in front of the face isn't solid
bool Tile_isFaceVisible(const Level* lvl, int x, int y, int z, int face);

void Tile_onDestroy(const Tile* self, Level* lvl, int x, int y, int z, ParticleEngine* engine);

// spawns the drops, each at a random point inside the broken block
void Tile_dropItems(const Tile* self, Level* lvl, int x, int y, int z);

#endif