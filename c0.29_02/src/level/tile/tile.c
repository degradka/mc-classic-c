// level/tile/tile.c

#include "tile.h"
#include "../../math_helper.h"
#include "../../particle/particle_engine.h"
#include "../../particle/particle.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "../../jrandom.h"

// one static Random for drops, sounds and hit particles
static JRandom gTileRandom;
static bool gTileRandomSeeded = false;

static JRandom* tileRandom(void) {
    if (!gTileRandomSeeded) {
        JRandom_initRandom(&gTileRandom);
        gTileRandomSeeded = true;
    }
    return &gTileRandom;
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const SoundTypeDef SOUND_TYPES[SOUND_TYPE_COUNT] = {
    { "-",      0.0f, 0.0f }, // SOUND_NONE
    { "grass",  0.6f, 1.0f }, // SOUND_GRASS
    { "grass",  0.7f, 1.2f }, // SOUND_CLOTH
    { "gravel", 1.0f, 1.0f }, // SOUND_GRAVEL
    { "stone",  1.0f, 1.0f }, // SOUND_STONE
    { "stone",  1.0f, 2.0f }, // SOUND_METAL
    { "wood",   1.0f, 1.0f }, // SOUND_WOOD
};

// randomized on every call
float SoundType_getVolume(SoundType t) {
    float r = JRandom_nextFloat(tileRandom());
    return SOUND_TYPES[t].volume / (r * 0.4f + 1.0f) * 0.5f;
}
float SoundType_getPitch(SoundType t) {
    float r = JRandom_nextFloat(tileRandom());
    return SOUND_TYPES[t].pitch / (r * 0.2f + 0.9f);
}

static int Tile_default_isSolid(const Tile* self)    { (void)self; return 1; }
static int Tile_default_blocksLight(const Tile* self){ (void)self; return 1; }
static int Tile_default_mayPick(const Tile* self)    { (void)self; return 1; }
static int Tile_default_getAABB(const Tile* self, int x,int y,int z, AABB* out) {
    if (out) *out = AABB_create(x + self->xx0, y + self->yy0, z + self->zz0, x + self->xx1, y + self->yy1, z + self->zz1);
    return 1;
}

static int Tile_default_getTexture(const Tile* self, int face) {
    (void)face;
    return self->textureId;
}

static int Tile_default_shouldRenderFace(const Tile* self, const Level* lvl, int x, int y, int z, int face) {
    (void)self; (void)face;
    return !Level_isSolidTile(lvl, x, y, z);
}

static void emitFace(int tex, Tessellator* t, int face,
                     float x0, float x1, float y0, float y1, float z0, float z1, float sy0, float sy1) {
    int xt = (tex % 16) * 16;
    int yt = (tex / 16) * 16;
    float u0 = xt / 256.0f;
    float u1 = (xt + 15.99f) / 256.0f;
    float v0 = yt / 256.0f;
    float v1 = (yt + 15.99f) / 256.0f;
    // sides take the shape's slice of the texture, unless the shape sticks out (liquids)
    if (face >= 2 && tex < 240 && !(sy0 < 0.0f || sy1 > 1.0f)) {
        v0 = ((float)yt + sy0 * 15.99f) / 256.0f;
        v1 = ((float)yt + sy1 * 15.99f) / 256.0f;
    }

    if (face == 0) {
        Tessellator_vertexUV(t, x0, y0, z1, u0, v1);
        Tessellator_vertexUV(t, x0, y0, z0, u0, v0);
        Tessellator_vertexUV(t, x1, y0, z0, u1, v0);
        Tessellator_vertexUV(t, x1, y0, z1, u1, v1);
    } else if (face == 1) {
        Tessellator_vertexUV(t, x1, y1, z1, u1, v1);
        Tessellator_vertexUV(t, x1, y1, z0, u1, v0);
        Tessellator_vertexUV(t, x0, y1, z0, u0, v0);
        Tessellator_vertexUV(t, x0, y1, z1, u0, v1);
    } else if (face == 2) {
        Tessellator_vertexUV(t, x0, y1, z0, u1, v0);
        Tessellator_vertexUV(t, x1, y1, z0, u0, v0);
        Tessellator_vertexUV(t, x1, y0, z0, u0, v1);
        Tessellator_vertexUV(t, x0, y0, z0, u1, v1);
    } else if (face == 3) {
        Tessellator_vertexUV(t, x0, y1, z1, u0, v0);
        Tessellator_vertexUV(t, x0, y0, z1, u0, v1);
        Tessellator_vertexUV(t, x1, y0, z1, u1, v1);
        Tessellator_vertexUV(t, x1, y1, z1, u1, v0);
    } else if (face == 4) {
        Tessellator_vertexUV(t, x0, y1, z1, u1, v0);
        Tessellator_vertexUV(t, x0, y1, z0, u0, v0);
        Tessellator_vertexUV(t, x0, y0, z0, u0, v1);
        Tessellator_vertexUV(t, x0, y0, z1, u1, v1);
    } else if (face == 5) {
        Tessellator_vertexUV(t, x1, y0, z1, u0, v1);
        Tessellator_vertexUV(t, x1, y0, z0, u1, v1);
        Tessellator_vertexUV(t, x1, y1, z0, u1, v0);
        Tessellator_vertexUV(t, x1, y1, z1, u0, v0);
    }
}

static void Tile_default_renderFace(const Tile* self, Tessellator* t, int x, int y, int z, int face) {
    emitFace(self->getTexture(self, face), t, face, x + self->xx0, x + self->xx1, y + self->yy0, y + self->yy1, z + self->zz0, z + self->zz1,
             self->yy0, self->yy1);
}

// Tile.renderFace with an explicit texture
void Tile_renderFaceTex(const Tile* self, Tessellator* t, int x, int y, int z, int face, int tex) {
    emitFace(tex, t, face, x + self->xx0, x + self->xx1, y + self->yy0, y + self->yy1, z + self->zz0, z + self->zz1,
             self->yy0, self->yy1);
}

// Tile.renderBackFace, the inside of a liquid surface
static void renderBackFace(const Tile* self, Tessellator* t, int x, int y, int z, int face) {
    int tex = self->getTexture(self, face);
    float u0 = (float)(tex % 16) / 16.0f;
    float u1 = u0 + 0.0624375f;
    float v0 = (float)(tex / 16) / 16.0f;
    float v1 = v0 + 0.0624375f;
    float x0 = x + self->xx0, x1 = x + self->xx1;
    float y0 = y + self->yy0, y1 = y + self->yy1;
    float z0 = z + self->zz0, z1 = z + self->zz1;
    if (face == 0) {
        Tessellator_vertexUV(t, x1, y0, z1, u1, v1);
        Tessellator_vertexUV(t, x1, y0, z0, u1, v0);
        Tessellator_vertexUV(t, x0, y0, z0, u0, v0);
        Tessellator_vertexUV(t, x0, y0, z1, u0, v1);
    }
    if (face == 1) {
        Tessellator_vertexUV(t, x0, y1, z1, u0, v1);
        Tessellator_vertexUV(t, x0, y1, z0, u0, v0);
        Tessellator_vertexUV(t, x1, y1, z0, u1, v0);
        Tessellator_vertexUV(t, x1, y1, z1, u1, v1);
    }
    if (face == 2) {
        Tessellator_vertexUV(t, x0, y0, z0, u1, v1);
        Tessellator_vertexUV(t, x1, y0, z0, u0, v1);
        Tessellator_vertexUV(t, x1, y1, z0, u0, v0);
        Tessellator_vertexUV(t, x0, y1, z0, u1, v0);
    }
    if (face == 3) {
        Tessellator_vertexUV(t, x1, y1, z1, u1, v0);
        Tessellator_vertexUV(t, x1, y0, z1, u1, v1);
        Tessellator_vertexUV(t, x0, y0, z1, u0, v1);
        Tessellator_vertexUV(t, x0, y1, z1, u0, v0);
    }
    if (face == 4) {
        Tessellator_vertexUV(t, x0, y0, z1, u1, v1);
        Tessellator_vertexUV(t, x0, y0, z0, u0, v1);
        Tessellator_vertexUV(t, x0, y1, z0, u0, v0);
        Tessellator_vertexUV(t, x0, y1, z1, u1, v0);
    }
    if (face == 5) {
        Tessellator_vertexUV(t, x1, y1, z1, u0, v0);
        Tessellator_vertexUV(t, x1, y1, z0, u1, v0);
        Tessellator_vertexUV(t, x1, y0, z0, u1, v1);
        Tessellator_vertexUV(t, x1, y0, z1, u0, v1);
    }
}

// lava is always fully bright
static float faceBrightness(const Tile* self, const Level* lvl, int x, int y, int z) {
    if (self->liquidType == LIQUID_LAVA) return 100.0f;
    return Level_getBrightness(lvl, x, y, z);
}

static bool Tile_default_render(const Tile* self, const Level* lvl, int x, int y, int z, Tessellator* t) {
    static const int dir[6][3] = { { 0, -1, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 }, { -1, 0, 0 }, { 1, 0, 0 } };
    static const float shade[6] = { 0.5f, 1.0f, 0.8f, 0.8f, 0.6f, 0.6f };
    bool drew = false;
    for (int face = 0; face < 6; ++face) {
        int nx = x + dir[face][0], ny = y + dir[face][1], nz = z + dir[face][2];
        if (!self->shouldRenderFace(self, lvl, nx, ny, nz, face)) continue;
        float b = faceBrightness(self, lvl, nx, ny, nz);
        Tessellator_color(t, shade[face] * b, shade[face] * b, shade[face] * b);
        self->renderFace(self, t, x, y, z, face);
        drew = true;
    }
    return drew;
}

// drawn at x -2 with fixed per face shading
static void Tile_default_renderGuiItem(const Tile* self, Tessellator* t) {
    static const float shade[6] = { 0.5f, 1.0f, 0.8f, 0.8f, 0.6f, 0.6f };
    for (int face = 0; face < 6; ++face) {
        Tessellator_color(t, shade[face], shade[face], shade[face]);
        self->renderFace(self, t, -2, 0, 0, face);
    }
}

// a normal per face, but the tesselator only draws at the end,
// so the whole block ends up lit with the last one
static void Tile_default_renderItem(const Tile* self, Tessellator* t) {
    Tessellator_begin(t);
    for (int face = 0; face < 6; ++face) self->renderFace(self, t, 0, 0, 0, face);
    glNormal3f(-1.0f, 0.0f, 0.0f);
    Tessellator_end(t);
}


// OreTile, coal ore really drops a slab
static int Ore_getDropCount(const Tile* self) { (void)self; return JRandom_nextIntN(tileRandom(), 3) + 1; }

// MetalTile
static int Metal_getTexture(const Tile* self, int face) {
    if (face == 1) return self->textureId - 16;
    if (face == 0) return self->textureId + 16;
    return self->textureId;
}

static int Slab_getTexture(const Tile* self, int face) { (void)self; return face <= 1 ? 6 : 5; }
static int Slab_isSolid(const Tile* self) { return self == &TILE_DOUBLE_SLAB; }
// a slab with anything on top becomes a double slab, a slab on a slab joins it
// this runs for the double slab too
static void Slab_onPlace(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    if (Level_getTile(lvl, x, y - 1, z) == TILE_SLAB.id) {
        level_setTile(lvl, x, y, z, 0);
        level_setTile(lvl, x, y - 1, z, TILE_DOUBLE_SLAB.id);
    }
}

static int Slab_shouldRenderFace(const Tile* self, const Level* lvl, int x, int y, int z, int face) {
    if (face == 1) return 1;
    if (!Tile_default_shouldRenderFace(self, lvl, x, y, z, face)) return 0;
    if (face == 0) return 1;
    return Level_getTile(lvl, x, y, z) != self->id;
}

static int Tnt_getTexture(const Tile* self, int face) {
    if (face == 0) return self->textureId + 2;
    if (face == 1) return self->textureId + 1;
    return self->textureId;
}
static int NoDrop_getDropCount(const Tile* self) { (void)self; return 0; }

static int Bookshelf_getTexture(const Tile* self, int face) { return face <= 1 ? 4 : self->textureId; }

/* tile instances and registry */

const Tile* gTiles[256] = { 0 };

static void Tile_default_neighborChanged(const Tile* self, Level* lvl, int x, int y, int z, int type) {
    (void)self; (void)lvl; (void)x; (void)y; (void)z; (void)type;
}

static int Tile_default_getDropCount(const Tile* self) { (void)self; return 1; }

static void registerTile(Tile* t, int id, int tex, int (*getTex)(const Tile*,int)) {
    t->id = id; t->textureId = tex;
    t->liquidType = LIQUID_NONE;
    t->soundType = SOUND_NONE;
    t->tileId = t->calmTileId = t->spreadSpeed = 0;
    t->isCube = true;
    t->explodable = true;
    t->tickDelay = 0;
    t->particleGravity = 1.0f;
    t->hardnessTicks = 20;
    t->xx0 = t->yy0 = t->zz0 = 0.0f;
    t->xx1 = t->yy1 = t->zz1 = 1.0f;
    t->getTexture = getTex ? getTex : Tile_default_getTexture;
    t->render = Tile_default_render;
    t->renderLayer = 0;
    t->onTick = NULL;
    t->isSolid     = Tile_default_isSolid;
    t->blocksLight = Tile_default_blocksLight;
    t->getAABB     = Tile_default_getAABB;
    t->mayPick     = Tile_default_mayPick;
    t->neighborChanged = Tile_default_neighborChanged;
    t->shouldRenderFace = Tile_default_shouldRenderFace;
    t->renderFace = Tile_default_renderFace;
    t->renderItem = Tile_default_renderItem;
    t->renderGuiItem = Tile_default_renderGuiItem;
    t->onPlace = NULL;
    t->onRemoved = NULL;
    t->getDropCount    = Tile_default_getDropCount;

    gTiles[id] = t;
}

Tile TILE_ROCK;
Tile TILE_DIRT;
Tile TILE_STONEBRICK;
Tile TILE_WOOD;
Tile TILE_GRASS;
Tile TILE_BUSH;
Tile TILE_BEDROCK;
Tile TILE_IRON_BLOCK;
Tile TILE_DOUBLE_SLAB;
Tile TILE_SLAB;
Tile TILE_BRICK;
Tile TILE_TNT;
Tile TILE_BOOKSHELF;
Tile TILE_MOSSY_STONE;
Tile TILE_OBSIDIAN;
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
Tile TILE_SPONGE;
Tile TILE_GLASS;

Tile TILE_CLOTH[16];
Tile TILE_DANDELION;
Tile TILE_ROSE;
Tile TILE_MUSHROOM_BROWN;
Tile TILE_MUSHROOM_RED;
Tile TILE_GOLD_BLOCK;

static int Grass_getTexture(const Tile* self, int face) {
    (void)self;
    return (face == 1) ? 0 : (face == 0) ? 2 : 3;
}

static void Grass_onTick(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    if (JRandom_nextIntN(&lvl->random, 4) != 0) return;

    if (!Level_isLit(lvl, x, y, z)) {
        level_setTile(lvl, x, y, z, TILE_DIRT.id);
    } else {
        // y spreads from -3 to +1
        for (int i = 0; i < 4; ++i) {
            int tx = x + JRandom_nextIntN(&lvl->random, 3) - 1;
            int ty = y + JRandom_nextIntN(&lvl->random, 5) - 3;
            int tz = z + JRandom_nextIntN(&lvl->random, 3) - 1;
            if (Level_getTile(lvl, tx, ty, tz) == TILE_DIRT.id && Level_isLit(lvl, tx, ty, tz)) {
                level_setTile(lvl, tx, ty, tz, TILE_GRASS.id);
            }
        }
    }
}

static int Bush_isSolid(const Tile* self)     { (void)self; return 0; }
static int Bush_blocksLight(const Tile* self) { (void)self; return 0; }
static int Bush_getAABB(const Tile* self, int x,int y,int z, AABB* out){
    (void)self; (void)x; (void)y; (void)z; (void)out;
    return 0;
}

static int Liquid_isSolid(const Tile* self) { (void)self; return 0; }
static int Liquid_getAABB(const Tile* self, int x,int y,int z, AABB* out){
    (void)self; (void)x; (void)y; (void)z; (void)out;
    return 0;
}
static int Liquid_mayPick(const Tile* self) { (void)self; return 0; }

// water stays out of the 5x5x5 around a sponge
static int hasNearbySponge(const Level* lvl, int x, int y, int z) {
    for (int dx = -2; dx <= 2; ++dx)
    for (int dy = -2; dy <= 2; ++dy)
    for (int dz = -2; dz <= 2; ++dz) {
        if (Level_getTile(lvl, x + dx, y + dy, z + dz) == TILE_SPONGE.id) return 1;
    }
    return 0;
}

static void Liquid_spreadToNeighbor(const Tile* self, Level* lvl, int x, int y, int z) {
    if (self->liquidType == LIQUID_WATER && hasNearbySponge(lvl, x, y, z)) return;
    if (Level_getTile(lvl, x, y, z) == 0 && level_setTile(lvl, x, y, z, self->tileId)) {
        Level_addToTickNextTick(lvl, x, y, z, self->tileId);
    }
}

static void Liquid_tick(const Tile* self, Level* lvl, int x, int y, int z) {
    // lava drops one block per tick, water falls until it lands
    bool changed = false;
    while (true) {
        y--;
        if (Level_getTile(lvl, x, y, z) != 0) break;
        if (self->liquidType == LIQUID_WATER && hasNearbySponge(lvl, x, y, z)) break;
        bool placed = level_setTile(lvl, x, y, z, self->tileId);
        if (placed) changed = true;
        if (!placed || self->liquidType == LIQUID_LAVA) break;
    }
    y++;

    if (self->liquidType == LIQUID_WATER || !changed) {
        Liquid_spreadToNeighbor(self, lvl, x - 1, y, z);
        Liquid_spreadToNeighbor(self, lvl, x + 1, y, z);
        Liquid_spreadToNeighbor(self, lvl, x, y, z - 1);
        Liquid_spreadToNeighbor(self, lvl, x, y, z + 1);
    }

    if (!changed) {
        Level_setTileNoUpdate(lvl, x, y, z, self->calmTileId);
    } else {
        Level_addToTickNextTick(lvl, x, y, z, self->tileId);
    }
}

static bool liquidsTurnToRock(const Tile* self, int type) {
    if (type == 0) return false;
    int other = gTiles[type] ? gTiles[type]->liquidType : LIQUID_NONE;
    return (self->liquidType == LIQUID_WATER && other == LIQUID_LAVA) ||
           (other == LIQUID_WATER && self->liquidType == LIQUID_LAVA);
}

static void Liquid_neighborChanged(const Tile* self, Level* lvl, int x, int y, int z, int type) {
    if (liquidsTurnToRock(self, type)) {
        level_setTile(lvl, x, y, z, TILE_ROCK.id);
        return;
    }
    Level_addToTickNextTick(lvl, x, y, z, type);
}

// no face against the same liquid
static int Liquid_shouldRenderFace(const Tile* self, const Level* lvl, int x, int y, int z, int face) {
    if (x < 0 || y < 0 || z < 0 || x >= lvl->width || z >= lvl->height) return 0;
    int id = Level_getTile(lvl, x, y, z);
    if (id == self->tileId || id == self->calmTileId) return 0;
    if (face == 1 && (Level_getTile(lvl, x - 1, y, z) == 0 || Level_getTile(lvl, x + 1, y, z) == 0 ||
                      Level_getTile(lvl, x, y, z - 1) == 0 || Level_getTile(lvl, x, y, z + 1) == 0)) return 1;
    return Tile_default_shouldRenderFace(self, lvl, x, y, z, face);
}

static void Liquid_renderFace(const Tile* self, Tessellator* t, int x, int y, int z, int face) {
    Tile_default_renderFace(self, t, x, y, z, face);
    renderBackFace(self, t, x, y, z, face);
}

static void Liquid_onBlockAdded(const Tile* self, Level* lvl, int x, int y, int z) {
    Level_addToTickNextTick(lvl, x, y, z, self->tileId);
}

static void CalmLiquid_neighborChanged(const Tile* self, Level* lvl, int x, int y, int z, int type) {
    bool hasAirNeighbor = false;
    if (Level_getTile(lvl, x - 1, y, z) == 0) hasAirNeighbor = true;
    if (Level_getTile(lvl, x + 1, y, z) == 0) hasAirNeighbor = true;
    if (Level_getTile(lvl, x, y, z - 1) == 0) hasAirNeighbor = true;
    if (Level_getTile(lvl, x, y, z + 1) == 0) hasAirNeighbor = true;
    if (Level_getTile(lvl, x, y - 1, z) == 0) hasAirNeighbor = true;
    if (liquidsTurnToRock(self, type)) {
        level_setTile(lvl, x, y, z, TILE_ROCK.id);
        return;
    }
    if (hasAirNeighbor) {
        Level_setTileNoUpdate(lvl, x, y, z, self->tileId);
        Level_addToTickNextTick(lvl, x, y, z, self->tileId);
    }
}

static int Liquid_getDropCount(const Tile* self) { (void)self; return 0; }

static void emitCross(const Tile* self, Tessellator* t, float x, float y, float z) {
    int tex = self->textureId;
    int xt = tex % 16 << 4;
    int yt = tex / 16 << 4;
    float u0 = (float)xt / 256.0f;
    float u1 = ((float)xt + 15.99f) / 256.0f;
    float v0 = (float)yt / 256.0f;
    float v1 = ((float)yt + 15.99f) / 256.0f;
    for (int r = 0; r < 2; ++r) {
        float xa = (float)((double)MathHelper_sin((float)r * 3.1415927f / 2.0f + 0.7853982f) * 0.5);
        float za = (float)((double)MathHelper_cos((float)r * 3.1415927f / 2.0f + 0.7853982f) * 0.5);
        float x0 = x + 0.5f - xa, x1 = x + 0.5f + xa;
        float y0 = y, y1 = y + 1.0f;
        float z0 = z + 0.5f - za, z1 = z + 0.5f + za;
        Tessellator_vertexUV(t, x0, y1, z0, u1, v0);
        Tessellator_vertexUV(t, x1, y1, z1, u0, v0);
        Tessellator_vertexUV(t, x1, y0, z1, u0, v1);
        Tessellator_vertexUV(t, x0, y0, z0, u1, v1);
        Tessellator_vertexUV(t, x1, y1, z1, u1, v0);
        Tessellator_vertexUV(t, x0, y1, z0, u0, v0);
        Tessellator_vertexUV(t, x0, y0, z0, u0, v1);
        Tessellator_vertexUV(t, x1, y0, z1, u1, v1);
    }
}

static bool Bush_render(const Tile* self, const Level* lvl, int x, int y, int z, Tessellator* t) {
    float b = Level_getBrightness(lvl, x, y, z);
    Tessellator_color(t, b, b, b);
    emitCross(self, t, (float)x, (float)y, (float)z);
    return true;
}

static void Bush_renderGuiItem(const Tile* self, Tessellator* t) {
    Tessellator_color(t, 1.0f, 1.0f, 1.0f);
    emitCross(self, t, -2.0f, 0.0f, 0.0f);
}

// the held plant
static void Bush_renderItem(const Tile* self, Tessellator* t) {
    glNormal3f(0.0f, 1.0f, 0.0f);
    Tessellator_begin(t);
    emitCross(self, t, 0.0f, 0.4f, -0.3f);
    Tessellator_end(t);
}

static void Flower_onTick(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    int below = Level_getTile(lvl, x, y - 1, z);
    if (!Level_isLit(lvl, x, y, z) || (below != TILE_DIRT.id && below != TILE_GRASS.id)) {
        level_setTile(lvl, x, y, z, 0);
    }
}

static void Mushroom_onTick(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    int below = Level_getTile(lvl, x, y - 1, z);
    if (Level_isLit(lvl, x, y, z) || (below != TILE_ROCK.id && below != TILE_GRAVEL.id && below != TILE_STONEBRICK.id)) {
        level_setTile(lvl, x, y, z, 0);
    }
}

static void Bush_onTick(const Tile* self, Level* lvl, int x, int y, int z) {
    int below = Level_getTile(lvl, x, y-1, z);
    if (!Level_isLit(lvl, x, y, z) || (below != TILE_DIRT.id && below != TILE_GRASS.id)) {
        level_setTile(lvl, x, y, z, 0);
        return;
    }
    if (JRandom_nextIntN(&lvl->random, 5) == 0) {
        Level_setTileNoUpdate(lvl, x, y, z, 0);
        if (!Level_maybeGrowTree(lvl, x, y, z)) {
            Level_setTileNoUpdate(lvl, x, y, z, self->id);
        }
    }
}

// sand and gravel fall right away when placed or when a neighbor changes
static void FallingTile_fall(Level* lvl, int x, int y, int z) {
    int j = y;
    while (true) {
        int below = Level_getTile(lvl, x, j - 1, z);
        bool open = below == 0 || (gTiles[below] && gTiles[below]->liquidType != LIQUID_NONE);
        if (!open || j <= 0) break;
        j--;
    }
    if (j != y) {
        int landing = Level_getTile(lvl, x, j, z);
        if (landing > 0 && gTiles[landing] && gTiles[landing]->liquidType != LIQUID_NONE) {
            Level_setTileNoUpdate(lvl, x, j, z, 0);
        }
        Level_swap(lvl, x, y, z, x, j, z);
    }
}
static void FallingTile_onBlockAdded(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    FallingTile_fall(lvl, x, y, z);
}
static void FallingTile_neighborChanged(const Tile* self, Level* lvl, int x, int y, int z, int type) {
    (void)self; (void)type;
    FallingTile_fall(lvl, x, y, z);
}

static int Log_getTexture(const Tile* self, int face) {
    (void)self;
    return (face == 0 || face == 1) ? 21 : 20;
}

// 3-5 planks
static int Log_getDropCount(const Tile* self)    { (void)self; return JRandom_nextIntN(tileRandom(), 3) + 3; }

static int Leaves_isSolid(const Tile* self)     { (void)self; return 0; }
static int Leaves_blocksLight(const Tile* self) { (void)self; return 0; }
// a sapling 1 in 10
static int Leaves_getDropCount(const Tile* self)    { (void)self; return (JRandom_nextIntN(tileRandom(), 10) == 0) ? 1 : 0; }

static void Sponge_onPlace(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    // the server dries it and sends the changes back
    if (lvl->networkMode) return;
    for (int dx = -2; dx <= 2; ++dx)
    for (int dy = -2; dy <= 2; ++dy)
    for (int dz = -2; dz <= 2; ++dz) {
        int id = Level_getTile(lvl, x + dx, y + dy, z + dz);
        if (id == TILE_WATER.id || id == TILE_CALM_WATER.id) {
            Level_setTileNoNeighborChange(lvl, x + dx, y + dy, z + dz, 0);
        }
    }
}
static void Sponge_onRemoved(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    for (int dx = -2; dx <= 2; ++dx)
    for (int dy = -2; dy <= 2; ++dy)
    for (int dz = -2; dz <= 2; ++dz) {
        Level_updateNeighborsAt(lvl, x + dx, y + dy, z + dz, Level_getTile(lvl, x + dx, y + dy, z + dz));
    }
}

// not solid for light and face culling, still solid to walk into
static int Glass_blocksLight(const Tile* self) { (void)self; return 0; }
static int Glass_isSolid(const Tile* self) { (void)self; return 0; }
static int Glass_shouldRenderFace(const Tile* self, const Level* lvl, int x, int y, int z, int face) {
    if (Level_getTile(lvl, x, y, z) == self->id) return 0;
    return Tile_default_shouldRenderFace(self, lvl, x, y, z, face);
}

void Tile_registerAll(void) {
    memset((void*)gTiles, 0, sizeof(gTiles));
    registerTile(&TILE_ROCK,       1,  1,  NULL);
    registerTile(&TILE_GRASS,      2,  3,  Grass_getTexture);
    registerTile(&TILE_DIRT,       3,  2,  NULL);
    registerTile(&TILE_STONEBRICK, 4, 16,  NULL);
    registerTile(&TILE_WOOD,       5,  4,  NULL);
    registerTile(&TILE_BUSH,       6, 15,  NULL);
    registerTile(&TILE_BEDROCK,    7, 17,  NULL);

    TILE_ROCK.soundType       = SOUND_STONE;
    TILE_GRASS.soundType      = SOUND_GRASS;
    TILE_DIRT.soundType       = SOUND_GRASS;
    TILE_STONEBRICK.soundType = SOUND_STONE;
    TILE_WOOD.soundType       = SOUND_WOOD;
    TILE_BEDROCK.soundType    = SOUND_STONE;

    TILE_ROCK.hardnessTicks       = 20;
    TILE_GRASS.hardnessTicks      = 12;
    TILE_DIRT.hardnessTicks       = 10;
    TILE_STONEBRICK.hardnessTicks = 30;
    TILE_WOOD.hardnessTicks       = 30;
    TILE_BUSH.hardnessTicks       = 0;
    TILE_BEDROCK.hardnessTicks    = 19980;
    TILE_ROCK.explodable = TILE_STONEBRICK.explodable = TILE_BEDROCK.explodable = false;
    TILE_BUSH.xx0 = TILE_BUSH.zz0 = 0.1f;
    TILE_BUSH.xx1 = TILE_BUSH.zz1 = 0.9f;
    TILE_BUSH.yy1 = 0.8f;

    TILE_GRASS.onTick     = Grass_onTick;
    TILE_GRASS.ticksRandomly = true;

    TILE_BUSH.isSolid     = Bush_isSolid;
    TILE_BUSH.blocksLight = Bush_blocksLight;
    TILE_BUSH.getAABB     = Bush_getAABB;
    TILE_BUSH.render      = Bush_render;
    TILE_BUSH.renderItem  = Bush_renderItem;
    TILE_BUSH.renderGuiItem = Bush_renderGuiItem;
    TILE_BUSH.onTick      = Bush_onTick;
    TILE_BUSH.ticksRandomly = true;
    TILE_BUSH.isCube = false;

    registerTile(&TILE_WATER,      8, 14, NULL);
    registerTile(&TILE_CALM_WATER, 9, 14, NULL);
    registerTile(&TILE_LAVA,      10, 30, NULL);
    registerTile(&TILE_CALM_LAVA, 11, 30, NULL);

    TILE_WATER.hardnessTicks      = 2000;
    TILE_CALM_WATER.hardnessTicks = 2000;
    TILE_LAVA.hardnessTicks       = 2000;
    TILE_CALM_LAVA.hardnessTicks  = 2000;

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

    // LiquidTile's shape, shifted by 0.01 and sunk by 0.1
    TILE_WATER.xx0 = TILE_CALM_WATER.xx0 = TILE_LAVA.xx0 = TILE_CALM_LAVA.xx0 = 0.01f + 0.0f;
    TILE_WATER.xx1 = TILE_CALM_WATER.xx1 = TILE_LAVA.xx1 = TILE_CALM_LAVA.xx1 = 0.01f + 1.0f;
    TILE_WATER.zz0 = TILE_CALM_WATER.zz0 = TILE_LAVA.zz0 = TILE_CALM_LAVA.zz0 = 0.01f + 0.0f;
    TILE_WATER.zz1 = TILE_CALM_WATER.zz1 = TILE_LAVA.zz1 = TILE_CALM_LAVA.zz1 = 0.01f + 1.0f;
    TILE_WATER.yy0 = TILE_CALM_WATER.yy0 = TILE_LAVA.yy0 = TILE_CALM_LAVA.yy0 = 0.0f - 0.1f + 0.01f;
    TILE_WATER.yy1 = TILE_CALM_WATER.yy1 = TILE_LAVA.yy1 = TILE_CALM_LAVA.yy1 = 1.0f - 0.1f + 0.01f;

    TILE_WATER.shouldRenderFace      = Liquid_shouldRenderFace;
    TILE_CALM_WATER.shouldRenderFace = Liquid_shouldRenderFace;
    TILE_LAVA.shouldRenderFace       = Liquid_shouldRenderFace;
    TILE_CALM_LAVA.shouldRenderFace  = Liquid_shouldRenderFace;

    TILE_WATER.renderLayer = TILE_CALM_WATER.renderLayer = 1;
    TILE_WATER.renderFace      = Liquid_renderFace;
    TILE_CALM_WATER.renderFace = Liquid_renderFace;
    TILE_LAVA.renderFace       = Liquid_renderFace;
    TILE_CALM_LAVA.renderFace  = Liquid_renderFace;

    TILE_WATER.tileId = TILE_WATER.id; TILE_WATER.calmTileId = TILE_CALM_WATER.id; TILE_WATER.spreadSpeed = 8;
    TILE_LAVA.tileId  = TILE_LAVA.id;  TILE_LAVA.calmTileId  = TILE_CALM_LAVA.id;  TILE_LAVA.spreadSpeed  = 2;
    TILE_CALM_WATER.tileId = TILE_WATER.id; TILE_CALM_WATER.calmTileId = TILE_CALM_WATER.id; TILE_CALM_WATER.spreadSpeed = 8;
    TILE_CALM_LAVA.tileId  = TILE_LAVA.id;  TILE_CALM_LAVA.calmTileId  = TILE_CALM_LAVA.id;  TILE_CALM_LAVA.spreadSpeed  = 2;

    TILE_LAVA.tickDelay      = 5;
    TILE_CALM_LAVA.tickDelay = 5;

    TILE_WATER.onTick = Liquid_tick;
    TILE_LAVA.onTick  = Liquid_tick;
    TILE_CALM_WATER.onTick = TILE_CALM_LAVA.onTick = Liquid_tick;
    TILE_WATER.ticksRandomly = TILE_LAVA.ticksRandomly = true;
    TILE_WATER.isCube = TILE_LAVA.isCube = TILE_CALM_WATER.isCube = TILE_CALM_LAVA.isCube = false;
    TILE_WATER.getDropCount = TILE_LAVA.getDropCount = Liquid_getDropCount;
    TILE_CALM_WATER.getDropCount = TILE_CALM_LAVA.getDropCount = Liquid_getDropCount;
    // calm variants don't tick

    TILE_WATER.neighborChanged      = Liquid_neighborChanged;
    TILE_LAVA.neighborChanged       = Liquid_neighborChanged;
    TILE_CALM_WATER.neighborChanged = CalmLiquid_neighborChanged;
    TILE_CALM_LAVA.neighborChanged  = CalmLiquid_neighborChanged;
    TILE_WATER.onBlockAdded = TILE_LAVA.onBlockAdded = Liquid_onBlockAdded;
    TILE_CALM_WATER.onBlockAdded = TILE_CALM_LAVA.onBlockAdded = Liquid_onBlockAdded;

    registerTile(&TILE_SAND,       12, 18, NULL);
    registerTile(&TILE_GRAVEL,     13, 19, NULL);
    registerTile(&TILE_GOLD_ORE,   14, 32, NULL);
    registerTile(&TILE_IRON_ORE,   15, 33, NULL);
    registerTile(&TILE_COAL_ORE,   16, 34, NULL);
    // 20 is the bark, the break particles use it
    registerTile(&TILE_LOG,        17, 20, Log_getTexture);
    registerTile(&TILE_LEAVES,     18, 22, NULL);

    TILE_SAND.hardnessTicks     = 10;
    TILE_GRAVEL.hardnessTicks   = 12;
    TILE_GOLD_ORE.hardnessTicks = 60;
    TILE_IRON_ORE.hardnessTicks = 60;
    TILE_COAL_ORE.hardnessTicks = 60;
    TILE_GOLD_ORE.explodable = TILE_IRON_ORE.explodable = TILE_COAL_ORE.explodable = false;
    TILE_GOLD_ORE.getDropCount = TILE_IRON_ORE.getDropCount = TILE_COAL_ORE.getDropCount = Ore_getDropCount;
    TILE_LOG.hardnessTicks      = 50;
    TILE_LEAVES.hardnessTicks   = 4;

    TILE_SAND.neighborChanged = TILE_GRAVEL.neighborChanged = FallingTile_neighborChanged;
    TILE_SAND.onBlockAdded = TILE_GRAVEL.onBlockAdded = FallingTile_onBlockAdded;

    TILE_LEAVES.isSolid     = Leaves_isSolid;
    TILE_LEAVES.blocksLight = Leaves_blocksLight;
    TILE_LEAVES.particleGravity = 0.4f;

    TILE_LOG.getDropCount       = Log_getDropCount;
    TILE_LEAVES.getDropCount    = Leaves_getDropCount;

    TILE_SAND.soundType     = SOUND_GRAVEL;
    TILE_GRAVEL.soundType   = SOUND_GRAVEL;
    TILE_GOLD_ORE.soundType = SOUND_STONE;
    TILE_IRON_ORE.soundType = SOUND_STONE;
    TILE_COAL_ORE.soundType = SOUND_STONE;
    TILE_LOG.soundType      = SOUND_WOOD;
    TILE_LEAVES.soundType   = SOUND_GRASS;

    registerTile(&TILE_SPONGE, 19, 48, NULL);
    registerTile(&TILE_GLASS,  20, 49, NULL);

    TILE_SPONGE.hardnessTicks = 12;
    TILE_GLASS.hardnessTicks  = 6;

    TILE_SPONGE.onPlace   = Sponge_onPlace;
    TILE_SPONGE.onRemoved = Sponge_onRemoved;
    TILE_SPONGE.soundType = SOUND_CLOTH;
    TILE_SPONGE.particleGravity = 0.9f;

    TILE_GLASS.blocksLight     = Glass_blocksLight;
    TILE_GLASS.isSolid         = Glass_isSolid;
    TILE_GLASS.shouldRenderFace = Glass_shouldRenderFace;
    // glass sounds like metal
    TILE_GLASS.soundType       = SOUND_METAL;

    for (int i = 0; i < 16; ++i) {
        registerTile(&TILE_CLOTH[i], 21 + i, 64 + i, NULL);
        TILE_CLOTH[i].soundType = SOUND_CLOTH;
        TILE_CLOTH[i].hardnessTicks = 16;
    }

    registerTile(&TILE_DANDELION,      37, 13, NULL);
    registerTile(&TILE_ROSE,           38, 12, NULL);
    registerTile(&TILE_MUSHROOM_BROWN, 39, 29, NULL);
    registerTile(&TILE_MUSHROOM_RED,   40, 28, NULL);

    Tile* plants[] = { &TILE_DANDELION, &TILE_ROSE, &TILE_MUSHROOM_BROWN, &TILE_MUSHROOM_RED };
    for (int i = 0; i < 4; ++i) {
        plants[i]->isSolid     = Bush_isSolid;
        plants[i]->blocksLight = Bush_blocksLight;
        plants[i]->getAABB     = Bush_getAABB;
        plants[i]->render      = Bush_render;
        plants[i]->renderItem  = Bush_renderItem;
        plants[i]->renderGuiItem = Bush_renderGuiItem;
        plants[i]->ticksRandomly = true;
        plants[i]->isCube = false;
        plants[i]->hardnessTicks = 0;
    }
    for (int i = 0; i < 4; ++i) {
        float r = 0.2f;
        plants[i]->xx0 = plants[i]->zz0 = 0.5f - r;
        plants[i]->xx1 = plants[i]->zz1 = r + 0.5f;
        plants[i]->yy1 = i < 2 ? r * 3.0f : r * 2.0f;
    }
    TILE_DANDELION.onTick = TILE_ROSE.onTick = Flower_onTick;
    TILE_MUSHROOM_BROWN.onTick = TILE_MUSHROOM_RED.onTick = Mushroom_onTick;

    registerTile(&TILE_GOLD_BLOCK, 41, 40, Metal_getTexture);
    TILE_GOLD_BLOCK.soundType = SOUND_METAL;
    TILE_GOLD_BLOCK.hardnessTicks = 60;
    TILE_GOLD_BLOCK.explodable = false;
    registerTile(&TILE_IRON_BLOCK, 42, 39, Metal_getTexture);
    TILE_IRON_BLOCK.soundType = SOUND_METAL;
    TILE_IRON_BLOCK.hardnessTicks = 100;
    TILE_IRON_BLOCK.explodable = false;

    Tile* slabs[] = { &TILE_DOUBLE_SLAB, &TILE_SLAB };
    for (int i = 0; i < 2; ++i) {
        registerTile(slabs[i], 43 + i, 6, Slab_getTexture);
        slabs[i]->soundType = SOUND_STONE;
        slabs[i]->hardnessTicks = 40;
        slabs[i]->explodable = false;
        slabs[i]->isSolid = Slab_isSolid;
        slabs[i]->onPlace = Slab_onPlace;
        slabs[i]->shouldRenderFace = Slab_shouldRenderFace;
    }
    TILE_SLAB.yy1 = 0.5f;
    TILE_SLAB.isCube = false;

    registerTile(&TILE_BRICK, 45, 7, NULL);
    TILE_BRICK.soundType = SOUND_STONE;
    TILE_BRICK.hardnessTicks = 40;
    TILE_BRICK.explodable = false;

    registerTile(&TILE_TNT, 46, 8, Tnt_getTexture);
    TILE_TNT.soundType = SOUND_CLOTH;
    TILE_TNT.hardnessTicks = 0;
    TILE_TNT.getDropCount = NoDrop_getDropCount;

    registerTile(&TILE_BOOKSHELF, 47, 35, Bookshelf_getTexture);
    TILE_BOOKSHELF.soundType = SOUND_WOOD;
    TILE_BOOKSHELF.hardnessTicks = 30;
    TILE_BOOKSHELF.getDropCount = NoDrop_getDropCount;

    registerTile(&TILE_MOSSY_STONE, 48, 36, NULL);
    TILE_MOSSY_STONE.soundType = SOUND_STONE;
    TILE_MOSSY_STONE.hardnessTicks = 20;
    TILE_MOSSY_STONE.explodable = false;

    registerTile(&TILE_OBSIDIAN, 49, 37, NULL);
    TILE_OBSIDIAN.soundType = SOUND_STONE;
    TILE_OBSIDIAN.hardnessTicks = 200;
    TILE_OBSIDIAN.explodable = false;
}

void Tile_onDestroy(const Tile* self, Level* lvl, int x, int y, int z, ParticleEngine* engine) {
    if (self == &TILE_TNT && !lvl->creativeMode) return;
    const int spread = 4;

    for (int ox = 0; ox < spread; ++ox)
    for (int oy = 0; oy < spread; ++oy)
    for (int oz = 0; oz < spread; ++oz) {
        float tx = x + (ox + 0.5f) / (float)spread;
        float ty = y + (oy + 0.5f) / (float)spread;
        float tz = z + (oz + 0.5f) / (float)spread;

        float mx = tx - x - 0.5f;
        float my = ty - y - 0.5f;
        float mz = tz - z - 0.5f;

        Particle p;
        Particle_initTerrain(&p, lvl, tx, ty, tz, mx, my, mz, self);
        ParticleEngine_add(engine, &p);
    }
}

// punch particle while mining, one per tick on the face being hit
void Tile_spawnHitParticle(const Tile* self, Level* lvl, int x, int y, int z, int face, ParticleEngine* engine) {
    float inset = 0.1f;
    float px = (float)x + JRandom_nextFloat(tileRandom()) * (self->xx1 - self->xx0 - inset * 2.0f) + inset + self->xx0;
    float py = (float)y + JRandom_nextFloat(tileRandom()) * (self->yy1 - self->yy0 - inset * 2.0f) + inset + self->yy0;
    float pz = (float)z + JRandom_nextFloat(tileRandom()) * (self->zz1 - self->zz0 - inset * 2.0f) + inset + self->zz0;
    if (face == 0) py = (float)y + self->yy0 - inset;
    if (face == 1) py = (float)y + self->yy1 + inset;
    if (face == 2) pz = (float)z + self->zz0 - inset;
    if (face == 3) pz = (float)z + self->zz1 + inset;
    if (face == 4) px = (float)x + self->xx0 - inset;
    if (face == 5) px = (float)x + self->xx1 + inset;
    Particle p;
    Particle_initTerrain(&p, lvl, px, py, pz, 0.0f, 0.0f, 0.0f, self);
    Particle_setPower(&p, 0.2f);
    Particle_scale(&p, 0.6f);
    ParticleEngine_add(engine, &p);
}

void Tile_dropItemsChance(const Tile* self, Level* lvl, int x, int y, int z, float chance) {
    (void)lvl; (void)x; (void)y; (void)z;
    int count = self->getDropCount(self);
    for (int i = 0; i < count; i++) {
        if (JRandom_nextFloat(tileRandom()) > chance) continue;
        JRandom_nextFloat(tileRandom());
        JRandom_nextFloat(tileRandom());
        JRandom_nextFloat(tileRandom());
    }
}

void Tile_dropItems(const Tile* self, Level* lvl, int x, int y, int z) {
    Tile_dropItemsChance(self, lvl, x, y, z, 1.0f);
}