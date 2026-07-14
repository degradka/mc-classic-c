// level/tile/tile.c

#include "tile.h"
#include "../../particle/particle_engine.h"
#include "../../particle/particle.h"
#include <string.h>
#include <math.h>
#include "../../jrandom.h"

const SoundType SOUND_TYPES[7] = {
    { "-", 0.0f, 0.0f },
    { "grass", 0.6f, 1.0f },
    { "grass", 0.7f, 1.2f },
    { "gravel", 1.0f, 1.0f },
    { "stone", 1.0f, 1.0f },
    { "stone", 1.0f, 2.0f },
    { "wood", 1.0f, 1.0f },
};

// Tile.random, only the sound types use it
static JRandom tileRandom;

float SoundType_getVolume(const SoundType* st) {
    return st->volume / (JRandom_nextFloat(&tileRandom) * 0.4f + 1.0f) * 0.5f;
}

float SoundType_getPitch(const SoundType* st) {
    return st->pitch / (JRandom_nextFloat(&tileRandom) * 0.2f + 0.9f);
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int Tile_default_isSolid(const Tile* self)    { (void)self; return 1; }
static int Tile_default_blocksLight(const Tile* self){ (void)self; return 1; }
static int Tile_default_getAABB(const Tile* self, int x,int y,int z, AABB* out) {
    (void)self;
    if (out) *out = AABB_create(x, y, z, x+1, y+1, z+1);
    return 1; // has AABB
}

static int Tile_default_getTexture(const Tile* self, int face) {
    (void)face;
    return self->textureId;
}

// layer 1 is the liquid pass, solid tiles only draw in layer 0
static int Tile_default_shouldRenderFace(const Tile* self, const Level* lvl, int x, int y, int z, int layer, int face) {
    (void)self; (void)face;
    if (layer == 1) return 0;
    return !Level_isSolidTile(lvl, x, y, z);
}


static void Tile_default_renderFace(const Tile* self, Tessellator* t, int x, int y, int z, int face) {
    int tex = self->getTexture(self, face);
    int xt = (tex % 16) * 16;
    int yt = (tex / 16) * 16;
    float u0 = xt / 256.0f;
    float u1 = (xt + 15.99f) / 256.0f;
    float v0 = yt / 256.0f;
    float v1 = (yt + 15.99f) / 256.0f;

    float x0 = x + self->xx0, x1 = x + self->xx1;
    float y0 = y + self->yy0, y1 = y + self->yy1;
    float z0 = z + self->zz0, z1 = z + self->zz1;

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

// reversed winding and the older uv padding, the back of a liquid surface
static void Tile_renderBackFace(const Tile* self, Tessellator* t, int x, int y, int z, int face) {
    int tex = self->getTexture(self, face);
    float u0 = (tex % 16) / 16.0f;
    float u1 = u0 + 0.0624375f;
    float v0 = (tex / 16) / 16.0f;
    float v1 = v0 + 0.0624375f;

    float x0 = x + self->xx0, x1 = x + self->xx1;
    float y0 = y + self->yy0, y1 = y + self->yy1;
    float z0 = z + self->zz0, z1 = z + self->zz1;

    if (face == 0) {
        Tessellator_vertexUV(t, x1, y0, z1, u1, v1);
        Tessellator_vertexUV(t, x1, y0, z0, u1, v0);
        Tessellator_vertexUV(t, x0, y0, z0, u0, v0);
        Tessellator_vertexUV(t, x0, y0, z1, u0, v1);
    } else if (face == 1) {
        Tessellator_vertexUV(t, x0, y1, z1, u0, v1);
        Tessellator_vertexUV(t, x0, y1, z0, u0, v0);
        Tessellator_vertexUV(t, x1, y1, z0, u1, v0);
        Tessellator_vertexUV(t, x1, y1, z1, u1, v1);
    } else if (face == 2) {
        Tessellator_vertexUV(t, x0, y0, z0, u1, v1);
        Tessellator_vertexUV(t, x1, y0, z0, u0, v1);
        Tessellator_vertexUV(t, x1, y1, z0, u0, v0);
        Tessellator_vertexUV(t, x0, y1, z0, u1, v0);
    } else if (face == 3) {
        Tessellator_vertexUV(t, x1, y1, z1, u1, v0);
        Tessellator_vertexUV(t, x1, y0, z1, u1, v1);
        Tessellator_vertexUV(t, x0, y0, z1, u0, v1);
        Tessellator_vertexUV(t, x0, y1, z1, u0, v0);
    } else if (face == 4) {
        Tessellator_vertexUV(t, x0, y0, z1, u1, v1);
        Tessellator_vertexUV(t, x0, y0, z0, u0, v1);
        Tessellator_vertexUV(t, x0, y1, z0, u0, v0);
        Tessellator_vertexUV(t, x0, y1, z1, u1, v0);
    } else if (face == 5) {
        Tessellator_vertexUV(t, x1, y1, z1, u0, v0);
        Tessellator_vertexUV(t, x1, y1, z0, u1, v0);
        Tessellator_vertexUV(t, x1, y0, z0, u1, v1);
        Tessellator_vertexUV(t, x1, y0, z1, u0, v1);
    }
}


static float Tile_default_getBrightness(const Tile* self, const Level* lvl, int x, int y, int z) {
    (void)self;
    return Level_getBrightness(lvl, x, y, z);
}

// each face is shaded by the brightness of the tile in front of it, the bottom at half
static bool Tile_render_shared(const Tile* self, Tessellator* t, const Level* lvl, int layer, int x, int y, int z) {
    bool drawn = false;
    float c1 = 0.0f;
    float c2 = 0.8f;
    float c3 = 0.6f;
    float br;

    if (self->shouldRenderFace(self, lvl, x, y - 1, z, layer, 0)) {
        c1 = 0.5f;
        br = self->getBrightness(self, lvl, x, y - 1, z);
        Tessellator_color(t, c1 * br, c1 * br, c1 * br);
        self->renderFace(self, t, x, y, z, 0);
        drawn = true;
    }
    if (self->shouldRenderFace(self, lvl, x, y + 1, z, layer, 1)) {
        br = self->getBrightness(self, lvl, x, y + 1, z);
        Tessellator_color(t, br * 1.0f, br * 1.0f, br * 1.0f);
        self->renderFace(self, t, x, y, z, 1);
        drawn = true;
    }
    if (self->shouldRenderFace(self, lvl, x, y, z - 1, layer, 2)) {
        br = self->getBrightness(self, lvl, x, y, z - 1);
        Tessellator_color(t, c2 * br, c2 * br, c2 * br);
        self->renderFace(self, t, x, y, z, 2);
        drawn = true;
    }
    if (self->shouldRenderFace(self, lvl, x, y, z + 1, layer, 3)) {
        br = self->getBrightness(self, lvl, x, y, z + 1);
        Tessellator_color(t, c2 * br, c2 * br, c2 * br);
        self->renderFace(self, t, x, y, z, 3);
        drawn = true;
    }
    if (self->shouldRenderFace(self, lvl, x - 1, y, z, layer, 4)) {
        br = self->getBrightness(self, lvl, x - 1, y, z);
        Tessellator_color(t, c3 * br, c3 * br, c3 * br);
        self->renderFace(self, t, x, y, z, 4);
        drawn = true;
    }
    if (self->shouldRenderFace(self, lvl, x + 1, y, z, layer, 5)) {
        br = self->getBrightness(self, lvl, x + 1, y, z);
        Tessellator_color(t, c3 * br, c3 * br, c3 * br);
        self->renderFace(self, t, x, y, z, 5);
        drawn = true;
    }
    return drawn;
}

// a face is pickable when the tile in front of it isn't solid
bool Tile_isFaceVisible(const Level* lvl, int x, int y, int z, int face) {
    if (face == 0) y--;
    if (face == 1) y++;
    if (face == 2) z--;
    if (face == 3) z++;
    if (face == 4) x--;
    if (face == 5) x++;
    return !Level_isSolidTile(lvl, x, y, z);
}

/* tile instances and registry */

const Tile* gTiles[256] = { 0 };

static void Tile_default_neighborChanged(const Tile* self, Level* lvl, int x, int y, int z, int type) {
    (void)self; (void)lvl; (void)x; (void)y; (void)z; (void)type;
}

static void Tile_default_onPlace(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self; (void)lvl; (void)x; (void)y; (void)z;
}

static void Tile_default_onAdded(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self; (void)lvl; (void)x; (void)y; (void)z;
}

static void Tile_default_onRemoved(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self; (void)lvl; (void)x; (void)y; (void)z;
}

static void registerTile(Tile* t, int id, int tex, int (*getTex)(const Tile*,int)) {
    t->id = id; t->textureId = tex;
    t->liquidType = LIQUID_NONE;
    t->tileId = t->calmTileId = 0;
    t->shouldTick = false;
    t->gravity = 1.0f;
    t->tickDelay = 0;
    t->getBrightness = Tile_default_getBrightness;
    t->xx0 = t->yy0 = t->zz0 = 0.0f;
    t->xx1 = t->yy1 = t->zz1 = 1.0f;
    t->getTexture = getTex ? getTex : Tile_default_getTexture;
    t->render = Tile_render_shared;
    t->onTick = NULL;
    t->isSolid     = Tile_default_isSolid;
    t->blocksLight = Tile_default_blocksLight;
    t->getAABB     = Tile_default_getAABB;
    t->soundType = SOUND_NONE;
    t->neighborChanged = Tile_default_neighborChanged;
    t->onPlace = Tile_default_onPlace;
    t->onAdded = Tile_default_onAdded;
    t->onRemoved = Tile_default_onRemoved;
    t->shouldRenderFace = Tile_default_shouldRenderFace;
    t->renderFace = Tile_default_renderFace;

    gTiles[id] = t;
}

Tile TILE_ROCK;
Tile TILE_GRASS;
Tile TILE_DIRT;
Tile TILE_STONEBRICK;
Tile TILE_WOOD;
Tile TILE_BUSH;
Tile TILE_UNBREAKABLE;
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
Tile TILE_BROWN_MUSHROOM;
Tile TILE_RED_MUSHROOM;
Tile TILE_GOLD_BLOCK;

static int Grass_getTexture(const Tile* self, int face) {
    (void)self;
    return (face == 1) ? 0 : (face == 0) ? 2 : 3;
}

static void Grass_onTick(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    if (JRandom_nextIntN(&lvl->random, 4) != 0) return;

    if (!Level_isLit(lvl, x, y + 1, z)) {
        level_setTile(lvl, x, y, z, TILE_DIRT.id);
    } else {
        for (int i = 0; i < 4; ++i) {
            int tx = x + JRandom_nextIntN(&lvl->random, 3) - 1;
            int ty = y + JRandom_nextIntN(&lvl->random, 5) - 3;
            int tz = z + JRandom_nextIntN(&lvl->random, 3) - 1;
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
    return 0;
}

static void Bush_onTick(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    int below = Level_getTile(lvl, x, y - 1, z);
    if (!Level_isLit(lvl, x, y, z) || (below != TILE_DIRT.id && below != TILE_GRASS.id)) {
        level_setTile(lvl, x, y, z, 0);
    }
}

static int Log_getTexture(const Tile* self, int face) {
    (void)self;
    if (face == 1) return 21;
    if (face == 0) return 21;
    return 20;
}

static int Leaves_isSolid(const Tile* self)     { (void)self; return 0; }
static int Leaves_blocksLight(const Tile* self) { (void)self; return 0; }

// sand and gravel drop straight down to the first tile below
// sand and gravel sink through water and lava too, swapping places with it
static void Falling_fall(Level* lvl, int x, int y, int z) {
    int ny = y;
    for (;;) {
        int id = Level_getTile(lvl, x, ny - 1, z);
        bool through = id == 0 || gTiles[id]->liquidType == LIQUID_WATER || gTiles[id]->liquidType == LIQUID_LAVA;
        if (!through || ny <= 0) break;
        ny--;
    }
    if (ny != y) Level_swap(lvl, x, y, z, x, ny, z);
}

static void Falling_onPlace(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    Falling_fall(lvl, x, y, z);
}

static void Falling_neighborChanged(const Tile* self, Level* lvl, int x, int y, int z, int type) {
    (void)self; (void)type;
    Falling_fall(lvl, x, y, z);
}

static int Liquid_isSolid(const Tile* self) { (void)self; return 0; }
static int Liquid_getAABB(const Tile* self, int x,int y,int z, AABB* out){
    (void)self; (void)x; (void)y; (void)z; (void)out;
    return 0;
}

static void Liquid_onPlace(const Tile* self, Level* lvl, int x, int y, int z) {
    Level_addToTickNextTick(lvl, x, y, z, self->tileId);
}

// water doesn't go within 2 of a sponge
static bool Liquid_canFlowTo(const Tile* self, const Level* lvl, int x, int y, int z) {
    if (self->liquidType != LIQUID_WATER) return true;
    for (int i = x - 2; i <= x + 2; i++)
    for (int j = y - 2; j <= y + 2; j++)
    for (int k = z - 2; k <= z + 2; k++) {
        if (Level_getTile(lvl, i, j, k) == TILE_SPONGE.id) return false;
    }
    return true;
}

// always says nothing changed, it only schedules the new tile
static bool Liquid_checkWater(const Tile* self, Level* lvl, int x, int y, int z) {
    if (Level_getTile(lvl, x, y, z) == 0) {
        if (!Liquid_canFlowTo(self, lvl, x, y, z)) return false;
        if (level_setTile(lvl, x, y, z, self->tileId)) Level_addToTickNextTick(lvl, x, y, z, self->tileId);
    }
    return false;
}

// falls through air, lava only one block, then spreads to the 4 sides
static void Liquid_onTick(const Tile* self, Level* lvl, int x, int y, int z) {
    bool hasChanged = false;
    while (Level_getTile(lvl, x, --y, z) == 0 && Liquid_canFlowTo(self, lvl, x, y, z)) {
        bool change = level_setTile(lvl, x, y, z, self->tileId);
        if (change) hasChanged = true;
        if (!change || self->liquidType == LIQUID_LAVA) break;
    }
    y++;
    if (self->liquidType == LIQUID_WATER || !hasChanged) {
        hasChanged |= Liquid_checkWater(self, lvl, x - 1, y, z);
        hasChanged |= Liquid_checkWater(self, lvl, x + 1, y, z);
        hasChanged |= Liquid_checkWater(self, lvl, x, y, z - 1);
        hasChanged |= Liquid_checkWater(self, lvl, x, y, z + 1);
    }
    if (!hasChanged) {
        Level_setTileNoUpdate(lvl, x, y, z, self->calmTileId);
    } else {
        Level_addToTickNextTick(lvl, x, y, z, self->tileId);
    }
}

static bool Liquid_meets(const Tile* self, int type) {
    if (type == 0) return false;
    int other = gTiles[type]->liquidType;
    return (self->liquidType == LIQUID_WATER && other == LIQUID_LAVA) ||
           (other == LIQUID_WATER && self->liquidType == LIQUID_LAVA);
}

// schedules a tick tagged with the neighbor's id, it only runs if that id ends up here
static void Liquid_neighborChanged(const Tile* self, Level* lvl, int x, int y, int z, int type) {
    if (Liquid_meets(self, type)) {
        level_setTile(lvl, x, y, z, TILE_ROCK.id);
        return;
    }
    Level_addToTickNextTick(lvl, x, y, z, type);
}

// lava lights itself, the colors just clamp
static float Liquid_getBrightness(const Tile* self, const Level* lvl, int x, int y, int z) {
    if (self->liquidType == LIQUID_LAVA) return 100.0f;
    return Level_getBrightness(lvl, x, y, z);
}

// water only shows in the liquid layer, lava in both
// no face against the same liquid
static int Liquid_shouldRenderFace(const Tile* self, const Level* lvl, int x, int y, int z, int layer, int face) {
    if (x < 0 || y < 0 || z < 0 || x >= lvl->width || z >= lvl->height) return 0;
    if (layer != 1 && self->liquidType == LIQUID_WATER) return 0;

    int id = Level_getTile(lvl, x, y, z);
    if (id == self->tileId || id == self->calmTileId) return 0;

    // the top is always drawn when there's air next to the liquid
    if (face == 1 && (Level_getTile(lvl, x - 1, y, z) == 0 || Level_getTile(lvl, x + 1, y, z) == 0 ||
                      Level_getTile(lvl, x, y, z - 1) == 0 || Level_getTile(lvl, x, y, z + 1) == 0)) {
        return 1;
    }
    return Tile_default_shouldRenderFace(self, lvl, x, y, z, -1, face);
}

static void Liquid_renderFace(const Tile* self, Tessellator* t, int x, int y, int z, int face) {
    Tile_default_renderFace(self, t, x, y, z, face);
    Tile_renderBackFace(self, t, x, y, z, face);
}

static void CalmLiquid_neighborChanged(const Tile* self, Level* lvl, int x, int y, int z, int type) {
    bool hasAirNeighbor = false;
    if (Level_getTile(lvl, x - 1, y, z) == 0) hasAirNeighbor = true;
    if (Level_getTile(lvl, x + 1, y, z) == 0) hasAirNeighbor = true;
    if (Level_getTile(lvl, x, y, z - 1) == 0) hasAirNeighbor = true;
    if (Level_getTile(lvl, x, y, z + 1) == 0) hasAirNeighbor = true;
    if (Level_getTile(lvl, x, y - 1, z) == 0) hasAirNeighbor = true;

    if (Liquid_meets(self, type)) {
        level_setTile(lvl, x, y, z, TILE_ROCK.id);
        return;
    }
    if (hasAirNeighbor) {
        Level_setTileNoUpdate(lvl, x, y, z, self->tileId);
        Level_addToTickNextTick(lvl, x, y, z, self->tileId);
    }
}

static void setupLiquid(Tile* t, int id, int liquidType, bool calm) {
    registerTile(t, id, liquidType == LIQUID_LAVA ? 30 : 14, NULL);
    t->liquidType = liquidType;
    t->tileId = calm ? id - 1 : id;
    t->calmTileId = calm ? id : id + 1;
    float f = 0.01f;
    float f2 = 0.1f;
    t->xx0 = f + 0.0f;
    t->yy0 = 0.0f - f2 + f;
    t->zz0 = f + 0.0f;
    t->xx1 = f + 1.0f;
    t->yy1 = 1.0f - f2 + f;
    t->zz1 = f + 1.0f;
    t->shouldTick = !calm;
    t->isSolid = Liquid_isSolid;
    t->getAABB = Liquid_getAABB;
    t->shouldRenderFace = Liquid_shouldRenderFace;
    t->renderFace = Liquid_renderFace;
    t->onPlace = Liquid_onPlace;
    t->onTick = calm ? NULL : Liquid_onTick;
    t->neighborChanged = calm ? CalmLiquid_neighborChanged : Liquid_neighborChanged;
    t->getBrightness = Liquid_getBrightness;
    if (liquidType == LIQUID_LAVA) t->tickDelay = 5;
}

// drying the water around it skips the neighbor updates
static void Sponge_onAdded(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    for (int i = x - 2; i <= x + 2; i++)
    for (int j = y - 2; j <= y + 2; j++)
    for (int k = z - 2; k <= z + 2; k++) {
        if (Level_isWater(lvl, i, j, k)) Level_setTileNoNeighborChange(lvl, i, j, k, 0);
    }
}

// every tile around it hears about its neighbors, so the water comes back
static void Sponge_onRemoved(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    for (int i = x - 2; i <= x + 2; i++)
    for (int j = y - 2; j <= y + 2; j++)
    for (int k = z - 2; k <= z + 2; k++) {
        Level_updateNeighborsAt(lvl, i, j, k, Level_getTile(lvl, i, j, k));
    }
}

static int Glass_isSolid(const Tile* self)     { (void)self; return 0; }
static int Glass_blocksLight(const Tile* self) { (void)self; return 0; }

// no faces between two glass tiles
static int Glass_shouldRenderFace(const Tile* self, const Level* lvl, int x, int y, int z, int layer, int face) {
    if (Level_getTile(lvl, x, y, z) == self->id) return 0;
    return Tile_default_shouldRenderFace(self, lvl, x, y, z, layer, face);
}

static bool Bush_render(const Tile* self, Tessellator* t, const Level* lvl,
                        int layer, int x, int y, int z)
{
    // layer 0 when lit, layer 1 (the liquid pass) when not
    const int lit = (x < 0 || y < 0 || z < 0 || x >= lvl->width || y >= lvl->depth || z >= lvl->height)
                    ? 1
                    : (y >= lvl->lightDepths[x + z * lvl->width]);
    if ( ((lit ^ (layer == 1)) == 0) ) return false;

    float tex = (float)self->textureId;
    float u0 = ((int)tex % 16) / 16.0f;
    float u1 = u0 + 0.0624375f;
    float v0 = ((int)tex / 16) / 16.0f;
    float v1 = v0 + 0.0624375f;

    Tessellator_color(t, 1.0f, 1.0f, 1.0f);

    const int rots = 2;
    for (int r = 0; r < rots; ++r) {
        float xa = (float)(sin(r * M_PI / rots + M_PI / 4.0) * 0.5);
        float za = (float)(cos(r * M_PI / rots + M_PI / 4.0) * 0.5);
        float x0 = x + 0.5f - xa, x1 = x + 0.5f + xa;
        float y0 = (float)y, y1 = (float)y + 1.0f;
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
    return true;
}


void Tile_registerAll(void) {
    memset((void*)gTiles, 0, sizeof(gTiles));
    registerTile(&TILE_ROCK,        1,  1, NULL);
    registerTile(&TILE_GRASS,       2,  3, Grass_getTexture);
    registerTile(&TILE_DIRT,        3,  2, NULL);
    registerTile(&TILE_STONEBRICK,  4, 16, NULL);
    registerTile(&TILE_WOOD,        5,  4, NULL);
    registerTile(&TILE_BUSH,        6, 15, NULL);
    registerTile(&TILE_UNBREAKABLE, 7, 17, NULL);
    setupLiquid(&TILE_WATER,      8, LIQUID_WATER, false);
    setupLiquid(&TILE_CALM_WATER, 9, LIQUID_WATER, true);
    setupLiquid(&TILE_LAVA,      10, LIQUID_LAVA, false);
    setupLiquid(&TILE_CALM_LAVA, 11, LIQUID_LAVA, true);
    registerTile(&TILE_SAND,       12, 18, NULL);
    registerTile(&TILE_GRAVEL,     13, 19, NULL);
    registerTile(&TILE_GOLD_ORE,   14, 32, NULL);
    registerTile(&TILE_IRON_ORE,   15, 33, NULL);
    registerTile(&TILE_COAL_ORE,   16, 34, NULL);
    registerTile(&TILE_LOG,        17, 20, Log_getTexture);
    registerTile(&TILE_LEAVES,     18, 22, NULL);
    registerTile(&TILE_SPONGE,     19, 48, NULL);
    registerTile(&TILE_GLASS,      20, 49, NULL);
    for (int i = 0; i < 16; i++) registerTile(&TILE_CLOTH[i], 21 + i, 64 + i, NULL);
    registerTile(&TILE_DANDELION,  37, 13, NULL);
    registerTile(&TILE_ROSE,       38, 12, NULL);
    registerTile(&TILE_BROWN_MUSHROOM, 39, 29, NULL);
    registerTile(&TILE_RED_MUSHROOM,   40, 28, NULL);
    registerTile(&TILE_GOLD_BLOCK, 41, 40, NULL);

    JRandom_initRandom(&tileRandom);
    static const int sounds[42] = {
        SOUND_NONE, SOUND_STONE, SOUND_GRASS, SOUND_GRASS, SOUND_STONE, SOUND_WOOD, SOUND_NONE, SOUND_STONE,
        SOUND_NONE, SOUND_NONE, SOUND_NONE, SOUND_NONE, SOUND_GRAVEL, SOUND_GRAVEL,
        SOUND_STONE, SOUND_STONE, SOUND_STONE, SOUND_WOOD, SOUND_GRASS, SOUND_CLOTH, SOUND_METAL,
        SOUND_CLOTH, SOUND_CLOTH, SOUND_CLOTH, SOUND_CLOTH, SOUND_CLOTH, SOUND_CLOTH, SOUND_CLOTH, SOUND_CLOTH,
        SOUND_CLOTH, SOUND_CLOTH, SOUND_CLOTH, SOUND_CLOTH, SOUND_CLOTH, SOUND_CLOTH, SOUND_CLOTH, SOUND_CLOTH,
        SOUND_NONE, SOUND_NONE, SOUND_NONE, SOUND_NONE, SOUND_METAL,
    };
    for (int i = 1; i < 42; i++) ((Tile*)gTiles[i])->soundType = sounds[i];

    TILE_GRASS.shouldTick = true;
    TILE_GRASS.onTick     = Grass_onTick;

    // flowers and mushrooms are bushes too
    Tile* bushes[5] = { &TILE_BUSH, &TILE_DANDELION, &TILE_ROSE, &TILE_BROWN_MUSHROOM, &TILE_RED_MUSHROOM };
    for (int i = 0; i < 5; i++) {
        bushes[i]->shouldTick  = true;
        bushes[i]->isSolid     = Bush_isSolid;
        bushes[i]->blocksLight = Bush_blocksLight;
        bushes[i]->getAABB     = Bush_getAABB;
        bushes[i]->render      = Bush_render;
        bushes[i]->onTick      = Bush_onTick;
    }

    TILE_SAND.onPlace           = Falling_onPlace;
    TILE_SAND.neighborChanged   = Falling_neighborChanged;
    TILE_GRAVEL.onPlace         = Falling_onPlace;
    TILE_GRAVEL.neighborChanged = Falling_neighborChanged;

    TILE_LEAVES.isSolid     = Leaves_isSolid;
    TILE_LEAVES.blocksLight = Leaves_blocksLight;
    TILE_LEAVES.gravity     = 0.4f;

    TILE_SPONGE.gravity   = 0.9f;
    TILE_SPONGE.onAdded   = Sponge_onAdded;
    TILE_SPONGE.onRemoved = Sponge_onRemoved;

    TILE_GLASS.isSolid          = Glass_isSolid;
    TILE_GLASS.blocksLight      = Glass_blocksLight;
    TILE_GLASS.shouldRenderFace = Glass_shouldRenderFace;
}

// highlight face, only drawn when the player is on its side
void Face_render(Tessellator* t, int x, int y, int z, int face, float px, float py, float pz) {
    const float minX = (float)x,     maxX = (float)x + 1.0f;
    const float minY = (float)y,     maxY = (float)y + 1.0f;
    const float minZ = (float)z,     maxZ = (float)z + 1.0f;

    if (face == 0 && (float)y > py) { // bottom
        Tessellator_vertex(t, minX, minY, maxZ);
        Tessellator_vertex(t, minX, minY, minZ);
        Tessellator_vertex(t, maxX, minY, minZ);
        Tessellator_vertex(t, maxX, minY, maxZ);
    } else if (face == 1 && (float)y < py) { // top
        Tessellator_vertex(t, maxX, maxY, maxZ);
        Tessellator_vertex(t, maxX, maxY, minZ);
        Tessellator_vertex(t, minX, maxY, minZ);
        Tessellator_vertex(t, minX, maxY, maxZ);
    } else if (face == 2 && (float)z > pz) { // negative Z
        Tessellator_vertex(t, minX, maxY, minZ);
        Tessellator_vertex(t, maxX, maxY, minZ);
        Tessellator_vertex(t, maxX, minY, minZ);
        Tessellator_vertex(t, minX, minY, minZ);
    } else if (face == 3 && (float)z < pz) { // positive Z
        Tessellator_vertex(t, minX, maxY, maxZ);
        Tessellator_vertex(t, minX, minY, maxZ);
        Tessellator_vertex(t, maxX, minY, maxZ);
        Tessellator_vertex(t, maxX, maxY, maxZ);
    } else if (face == 4 && (float)x > px) { // negative X
        Tessellator_vertex(t, minX, maxY, maxZ);
        Tessellator_vertex(t, minX, maxY, minZ);
        Tessellator_vertex(t, minX, minY, minZ);
        Tessellator_vertex(t, minX, minY, maxZ);
    } else if (face == 5 && (float)x < px) { // +X
        Tessellator_vertex(t, maxX, minY, maxZ);
        Tessellator_vertex(t, maxX, minY, minZ);
        Tessellator_vertex(t, maxX, maxY, minZ);
        Tessellator_vertex(t, maxX, maxY, maxZ);
    }
}

void Tile_onDestroy(const Tile* self, Level* lvl, int x, int y, int z, ParticleEngine* engine) {
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
        Particle_init(&p, lvl, tx, ty, tz, mx, my, mz, self->textureId, self->gravity);
        ParticleEngine_add(engine, &p);
    }
}