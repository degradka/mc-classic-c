// tile.c: registry, per face textures, render helpers

#include "tile.h"
#include "../../particle/particle_engine.h"
#include "../../particle/particle.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// c0.0.23a_01: Tile$SoundType table, matching the real source's enum exactly
const SoundTypeDef SOUND_TYPES[SOUND_TYPE_COUNT] = {
    { "-",      0.0f, 0.0f }, // SOUND_NONE
    { "grass",  0.6f, 1.0f }, // SOUND_GRASS
    { "grass",  0.7f, 1.2f }, // SOUND_CLOTH
    { "gravel", 1.0f, 1.0f }, // SOUND_GRAVEL
    { "stone",  1.0f, 1.0f }, // SOUND_STONE
    { "stone",  1.0f, 2.0f }, // SOUND_METAL
    { "wood",   1.0f, 1.0f }, // SOUND_WOOD
};

// matches Tile$SoundType.getVolume()/getPitch(): both apply their own random
// jitter on every call, not just once at registration
float SoundType_getVolume(SoundType t) {
    float r = (float)rand() / (float)RAND_MAX;
    return SOUND_TYPES[t].volume / (r * 0.4f + 1.0f) * 0.5f;
}
float SoundType_getPitch(SoundType t) {
    float r = (float)rand() / (float)RAND_MAX;
    return SOUND_TYPES[t].pitch / (r * 0.2f + 0.9f);
}

static int Tile_default_isSolid(const Tile* self)    { (void)self; return 1; }
static int Tile_default_blocksLight(const Tile* self){ (void)self; return 1; }
static int Tile_default_mayPick(const Tile* self)    { (void)self; return 1; }
static int Tile_default_getAABB(const Tile* self, int x,int y,int z, AABB* out) {
    (void)self;
    if (out) *out = AABB_create(x, y, z, x+1, y+1, z+1);
    return 1; // has AABB
}

static int Tile_default_getTexture(const Tile* self, int face) {
    (void)face;
    return self->textureId;
}

// c0.0.14a_08 merged the old lit/shadow two pass split into a single solid
// pass (layer 0), now colored by per face neighbor brightness instead of a
// binary lit/unlit choice. layer 1 is the liquid only layer (renumbered down
// from 2, since there is no more separate shadow pass), so a plain tile
// never shows there. c0.0.20a_02 added layer 2 for unlit cross-quad plants,
// a plain tile never shows there either.
static int Tile_default_shouldRenderFace(const Tile* self, const Level* lvl, int x, int y, int z, int layer, int face) {
    (void)self; (void)face;
    if (layer == 1 || layer == 2) return 0;
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

// c0.0.14a_08: per face color is now the neighbor cell's brightness
// (Level_getBrightness, 1.0 lit / 0.5 shadowed) times the same fixed
// directional factors as before, replacing the old flat lit/shadow colors.
// This is what puts real shadows onto every tile face including liquids,
// since liquids call this same shared renderer first.
static void Tile_render_shared(const Tile* self, Tessellator* t, const Level* lvl, int layer, int x, int y, int z) {
    const float c1 = 1.0f, c2 = 0.8f, c3 = 0.6f;

    if (self->shouldRenderFace(self, lvl, x, y - 1, z, layer, 0)) {
        float b = Level_getBrightness(lvl, x, y - 1, z);
        Tessellator_color(t, b * c1, b * c1, b * c1);
        self->renderFace(self, t, x, y, z, 0);
    }
    if (self->shouldRenderFace(self, lvl, x, y + 1, z, layer, 1)) {
        float b = Level_getBrightness(lvl, x, y + 1, z);
        Tessellator_color(t, b * c1, b * c1, b * c1);
        self->renderFace(self, t, x, y, z, 1);
    }
    if (self->shouldRenderFace(self, lvl, x, y, z - 1, layer, 2)) {
        float b = Level_getBrightness(lvl, x, y, z - 1);
        Tessellator_color(t, b * c2, b * c2, b * c2);
        self->renderFace(self, t, x, y, z, 2);
    }
    if (self->shouldRenderFace(self, lvl, x, y, z + 1, layer, 3)) {
        float b = Level_getBrightness(lvl, x, y, z + 1);
        Tessellator_color(t, b * c2, b * c2, b * c2);
        self->renderFace(self, t, x, y, z, 3);
    }
    if (self->shouldRenderFace(self, lvl, x - 1, y, z, layer, 4)) {
        float b = Level_getBrightness(lvl, x - 1, y, z);
        Tessellator_color(t, b * c3, b * c3, b * c3);
        self->renderFace(self, t, x, y, z, 4);
    }
    if (self->shouldRenderFace(self, lvl, x + 1, y, z, layer, 5)) {
        float b = Level_getBrightness(lvl, x + 1, y, z);
        Tessellator_color(t, b * c3, b * c3, b * c3);
        self->renderFace(self, t, x, y, z, 5);
    }
}

/* tile instances and registry */

const Tile* gTiles[256] = { 0 };

static void Tile_default_neighborChanged(const Tile* self, Level* lvl, int x, int y, int z, int type) {
    (void)self; (void)lvl; (void)x; (void)y; (void)z; (void)type;
}

static int Tile_default_getDropCount(const Tile* self) { (void)self; return 1; }
static int Tile_default_getDropResource(const Tile* self) { return self->id; }

static void registerTile(Tile* t, int id, int tex, int (*getTex)(const Tile*,int)) {
    t->id = id; t->textureId = tex;
    t->liquidType = LIQUID_NONE;
    t->soundType = SOUND_NONE;
    t->tileId = t->calmTileId = t->spreadSpeed = 0;
    t->tickDelay = 0;
    t->particleGravity = 1.0f;
    t->hardnessTicks = 20; // overridden per tile below, matches real source's own explicit hardness set at every registration
    t->xx0 = t->yy0 = t->zz0 = 0.0f;
    t->xx1 = t->yy1 = t->zz1 = 1.0f;
    t->getTexture = getTex ? getTex : Tile_default_getTexture;
    t->render = Tile_render_shared;
    t->onTick = NULL;
    t->isSolid     = Tile_default_isSolid;
    t->blocksLight = Tile_default_blocksLight;
    t->getAABB     = Tile_default_getAABB;
    t->mayPick     = Tile_default_mayPick;
    t->neighborChanged = Tile_default_neighborChanged;
    t->shouldRenderFace = Tile_default_shouldRenderFace;
    t->renderFace = Tile_default_renderFace;
    t->onPlace = NULL;
    t->onRemoved = NULL;
    t->getDropCount    = Tile_default_getDropCount;
    t->getDropResource = Tile_default_getDropResource;

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
Tile TILE_SPONGE;
Tile TILE_GLASS;

Tile TILE_CLOTH[16];
Tile TILE_DANDELION;
Tile TILE_ROSE;
Tile TILE_MUSHROOM_BROWN;
Tile TILE_MUSHROOM_RED;
Tile TILE_GOLD_BLOCK;

/* Grass has per face textures: top is 0, bottom is 2, sides are 3 */
static int Grass_getTexture(const Tile* self, int face) {
    (void)self;
    return (face == 1) ? 0 : (face == 0) ? 2 : 3;
}

// breaking Grass drops Dirt, not Grass itself, matching real source exactly:
// level/tile/k.java's Grass class overrides g() to return Dirt's own
// getDropResource() rather than its own id. Without this, Grass had no
// override at all and fell back to the generic drop self default, its own
// id of 2, handing the player a Grass block item that then landed in its
// own separate, never restacking hotbar slot instead of merging with
// genuine Dirt drops
static int Grass_getDropResource(const Tile* self) { (void)self; return TILE_DIRT.id; }

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

// c0.0.19a_04: true if a Sponge exists within a 5x5x5 cube centered on
// (x,y,z). Used to stop water (not lava) from falling or spreading into a
// sponge's dry zone, working together with Sponge's own onPlace/onRemoved
// hooks to keep water from re-flooding the area it just dried
static int hasNearbySponge(const Level* lvl, int x, int y, int z) {
    for (int dx = -2; dx <= 2; ++dx)
    for (int dy = -2; dy <= 2; ++dy)
    for (int dz = -2; dz <= 2; ++dz) {
        if (Level_getTile(lvl, x + dx, y + dy, z + dz) == TILE_SPONGE.id) return 1;
    }
    return 0;
}

// Spreads one cell into an air neighbor and schedules its own next tick,
// used by the falling/spreading pass below. Always returns nothing useful by
// design (matches the real source, which discards this helper's result and
// bases the calm/flowing decision purely on the fall loop below), the point
// is the side effect of placing a tile and scheduling its next tick.
static void Liquid_spreadToNeighbor(const Tile* self, Level* lvl, int x, int y, int z) {
    if (self->liquidType == LIQUID_WATER && hasNearbySponge(lvl, x, y, z)) return;
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
    // the loop fall forever and hangs the game. c0.0.19a_04: water also
    // stops falling into a cell within a sponge's 5x5x5 dry zone
    while (y > 0 && Level_getTile(lvl, x, y - 1, z) == 0
           && !(self->liquidType == LIQUID_WATER && hasNearbySponge(lvl, x, y - 1, z))) {
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

// Water only shows in the dedicated liquid layer (1, renumbered down from 2
// since the old shadow pass is gone), lava shows in every layer via the -1
// sentinel it passes to the base check. Skips the same liquid type on the
// neighboring tile so touching water or lava tiles don't draw an internal
// face between them.
static int Liquid_shouldRenderFace(const Tile* self, const Level* lvl, int x, int y, int z, int layer, int face) {
    // c0.0.20a_02: layer 2 is the unlit cross-quad plants list, never liquid
    if (layer == 2) return 0;
    if (x < 0 || y < 0 || z < 0 || x >= lvl->width || z >= lvl->height) return 0;
    if (layer != 1 && self->liquidType == LIQUID_WATER) return 0;

    int id = Level_getTile(lvl, x, y, z);
    if (id == self->tileId || id == self->calmTileId) return 0;

    return Tile_default_shouldRenderFace(self, lvl, x, y, z, -1, face);
}

static void Liquid_renderFace(const Tile* self, Tessellator* t, int x, int y, int z, int face) {
    // c0.0.20a_02: the real source also draws a second, reversed winding
    // copy of this same face right after (matches base class methods a()
    // then b()), meant to make the liquid surface visible from both sides.
    // Since GL_CULL_FACE is never enabled anywhere in this port (confirmed,
    // matches the real client), a single quad already renders from both
    // sides on its own, so the second, exactly coplanar copy is dropped
    // here: it served no visual purpose and caused the water/lava surface
    // to z fight with itself, showing as flicker and light colored seams
    Tile_default_renderFace(self, t, x, y, z, face);
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

static void Bush_render(const Tile* self, Tessellator* t, const Level* lvl,
                        int layer, int x, int y, int z)
{
    // c0.0.20a_02: lit renders into layer 0 (solid), unlit renders into its
    // own layer 2 instead of layer 1 (liquid). The real source puts unlit
    // cross-quad plants in the liquid list too, but that list gets drawn
    // twice per frame (once depth only, once with color, for correct water
    // transparency), and a plant's two crossing quads self-intersect, so
    // part of the sprite fails the second pass's depth test wherever they
    // overlap on screen. A separate, single-pass list avoids that
    const int lit = (x < 0 || y < 0 || z < 0 || x >= lvl->width || y >= lvl->depth || z >= lvl->height)
                    ? 1
                    : (y >= lvl->lightDepths[x + z * lvl->width]);
    if (lit ? (layer != 0) : (layer != 2)) return;

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

        // c0.0.20a_02: the real source draws this same quad a second time
        // right after, reversed (opposite winding), meant to make it visible
        // from both sides without relying on two sided rendering. Since
        // GL_CULL_FACE is never enabled anywhere in this port (confirmed,
        // matches the real client), a single quad already renders from both
        // sides on its own, so the second, exactly coplanar copy is dropped
        // here: it served no visual purpose and caused the two crossing
        // quads to z fight with themselves, flickering
        Tessellator_vertexUV(t, x0, y1, z0, u1, v0);
        Tessellator_vertexUV(t, x1, y1, z1, u0, v0);
        Tessellator_vertexUV(t, x1, y0, z1, u0, v1);
        Tessellator_vertexUV(t, x0, y0, z0, u1, v1);
    }
}

// c0.24_st_03: Bush is this codebase's own name for what the real source
// calls Sapling (level\tile\i.java) - same tile, id 6, same class hierarchy
static void Bush_onTick(const Tile* self, Level* lvl, int x, int y, int z) {
    int below = Level_getTile(lvl, x, y-1, z);
    if (!Level_isLit(lvl, x, y, z) || (below != TILE_DIRT.id && below != TILE_GRASS.id)) {
        level_setTile(lvl, x, y, z, 0);
        return;
    }
    // c0.24_st_03: 1-in-5 chance per random tick to attempt growing into a
    // tree. Silently clears/restores the sapling's own tile (setTileNoUpdate,
    // no neighbor notification for this single swap), but the tree itself
    // (Level_maybeGrowTree) sets every block through the normal notifying
    // path, matching the real source's own mix of the two exactly
    if (rand() % 5 == 0) {
        Level_setTileNoUpdate(lvl, x, y, z, 0);
        if (!Level_maybeGrowTree(lvl, x, y, z)) {
            Level_setTileNoUpdate(lvl, x, y, z, self->id);
        }
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

/* Log: new in c0.0.14a_08, per face texture, rings on top/bottom, bark on sides */
static int Log_getTexture(const Tile* self, int face) {
    (void)self;
    return (face == 0 || face == 1) ? 21 : 20;
}

// c0.24_st_03: drops 3-5 planks, matches level\tile\d.java's f()/g() exactly
static int Log_getDropCount(const Tile* self)    { (void)self; return rand() % 3 + 3; }
static int Log_getDropResource(const Tile* self) { (void)self; return TILE_WOOD.id; }

/* Leaves: new in c0.0.14a_08, non solid, non light blocking, planted by
   world generation's new tree pass */
static int Leaves_isSolid(const Tile* self)     { (void)self; return 0; }
static int Leaves_blocksLight(const Tile* self) { (void)self; return 0; }
// c0.24_st_03: 1-in-6 chance of a single sapling, matches level\tile\e.java's
// f()/g() exactly - returning a drop count of 0 the other 5/6 of the time
// naturally yields "no drop" via Tile_dropItems' own count-driven loop, no
// separate chance check needed on top of it
static int Leaves_getDropCount(const Tile* self)    { (void)self; return (rand() % 6 == 0) ? 1 : 0; }
static int Leaves_getDropResource(const Tile* self) { (void)self; return TILE_BUSH.id; }

/* Sponge: new in c0.0.19a_04. Dries a 5x5x5 area of water on placement, and
   re-triggers neighbor evaluation over the same area when removed (working
   with the water tile's own sponge-proximity guard above to keep the area
   from immediately re-flooding). No onTick override, so it cannot randomly
   decay via the ambient random-tick scheduler, matching the real source. */
static void Sponge_onPlace(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    // c0.0.19a_04: gated no-op in multiplayer, matching the real source
    // calling the gated setTileNoNeighborChange here (not the always
    // executing netSetTileNoNeighborChange). The server dries the area
    // itself and echoes each affected water tile back as its own SetBlock
    if (lvl->networkMode) return;
    for (int dx = -2; dx <= 2; ++dx)
    for (int dy = -2; dy <= 2; ++dy)
    for (int dz = -2; dz <= 2; ++dz) {
        int id = Level_getTile(lvl, x + dx, y + dy, z + dz);
        if (id == TILE_WATER.id || id == TILE_CALM_WATER.id) {
            Level_setTileNoUpdate(lvl, x + dx, y + dy, z + dz, 0);
        }
    }
}
static void Sponge_onRemoved(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    for (int dx = -2; dx <= 2; ++dx)
    for (int dy = -2; dy <= 2; ++dy)
    for (int dz = -2; dz <= 2; ++dz) {
        Level_updateNeighborsAt(lvl, x + dx, y + dy, z + dz);
    }
}

/* Glass: new in c0.0.19a_04. Doesn't block light, and doesn't count as
   "solid" for opacity/face-culling purposes either (a neighboring tile's
   face touching Glass renders normally rather than getting hidden behind an
   assumed-opaque neighbor). Physics stays solid: only the two no-arg
   blocksLight/isSolid overrides below, not the AABB method, so collision is
   unaffected, matching the real source overriding both a() and b() to false
   while leaving the 3-arg AABB getter untouched. Also skips its own face
   where the neighboring tile is also Glass, hiding the seam between
   adjacent glass blocks. */
static int Glass_blocksLight(const Tile* self) { (void)self; return 0; }
static int Glass_isSolid(const Tile* self) { (void)self; return 0; }
static int Glass_shouldRenderFace(const Tile* self, const Level* lvl, int x, int y, int z, int layer, int face) {
    if (Level_getTile(lvl, x, y, z) == self->id) return 0;
    return Tile_default_shouldRenderFace(self, lvl, x, y, z, layer, face);
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

    // c0.0.23a_01: Tile$SoundType assignments, read directly from the real
    // source's per tile constructor calls (not inferred)
    TILE_ROCK.soundType       = SOUND_STONE;
    TILE_GRASS.soundType      = SOUND_GRASS;
    TILE_DIRT.soundType       = SOUND_GRASS;
    TILE_STONEBRICK.soundType = SOUND_STONE;
    TILE_WOOD.soundType       = SOUND_WOOD;
    TILE_BEDROCK.soundType    = SOUND_STONE;

    // c0.24_st_03: real per tile hardness (seconds*20, read directly from
    // each tile's own constructor call), matching the real source's
    // progressive mining: hold the crosshair on the same block for
    // hardnessTicks+1 ticks before it actually breaks
    TILE_ROCK.hardnessTicks       = 20;
    TILE_GRASS.hardnessTicks      = 12;
    TILE_DIRT.hardnessTicks       = 10;
    TILE_STONEBRICK.hardnessTicks = 40;
    TILE_WOOD.hardnessTicks       = 40;
    TILE_BUSH.hardnessTicks       = 0; // instant, matches every plant/flower/mushroom
    TILE_BEDROCK.hardnessTicks    = 2000;

    TILE_GRASS.onTick     = Grass_onTick;
    TILE_GRASS.getDropResource = Grass_getDropResource;

    TILE_BUSH.isSolid     = Bush_isSolid;
    TILE_BUSH.blocksLight = Bush_blocksLight;
    TILE_BUSH.getAABB     = Bush_getAABB;
    TILE_BUSH.render      = Bush_render;
    TILE_BUSH.onTick      = Bush_onTick;

    // tex 14 = water, tex 30 = lava (terrain.png atlas slots, matching LiquidTile.java)
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

    // shape is a full block minus a thin sliver off the top, so the surface
    // sits slightly below a full block. c0.0.19a_04 widened the horizontal
    // extent by 0.01 past the voxel cube on every side (was an exact [0,1]),
    // enabled by a previously broken shape setter being fixed to actually
    // respect all 6 parameters instead of hardcoding X/Z, and that 0.01
    // widening is confirmed byte-identical in c0.0.20a_02's own decompile
    // too. c0.0.20a_02: removed here anyway, since it makes adjacent liquid
    // tiles' faces overlap by 0.02 at their shared border, which is a clean,
    // well evidenced cause for exactly the shimmering light colored border
    // seams the wiki calls out as fixed in this version ("no longer has
    // light blue bits in their borders"); whatever the real fix actually
    // was, it isn't reachable in the decompiled source available here,
    // matching the same situation as the liquid double face draw above
    TILE_WATER.xx0 = TILE_CALM_WATER.xx0 = TILE_LAVA.xx0 = TILE_CALM_LAVA.xx0 = 0.0f;
    TILE_WATER.xx1 = TILE_CALM_WATER.xx1 = TILE_LAVA.xx1 = TILE_CALM_LAVA.xx1 = 1.0f;
    TILE_WATER.zz0 = TILE_CALM_WATER.zz0 = TILE_LAVA.zz0 = TILE_CALM_LAVA.zz0 = 0.0f;
    TILE_WATER.zz1 = TILE_CALM_WATER.zz1 = TILE_LAVA.zz1 = TILE_CALM_LAVA.zz1 = 1.0f;
    TILE_WATER.yy0 = TILE_CALM_WATER.yy0 = TILE_LAVA.yy0 = TILE_CALM_LAVA.yy0 = -0.1f;
    TILE_WATER.yy1 = TILE_CALM_WATER.yy1 = TILE_LAVA.yy1 = TILE_CALM_LAVA.yy1 = 0.9f;

    TILE_WATER.shouldRenderFace      = Liquid_shouldRenderFace;
    TILE_CALM_WATER.shouldRenderFace = Liquid_shouldRenderFace;
    TILE_LAVA.shouldRenderFace       = Liquid_shouldRenderFace;
    TILE_CALM_LAVA.shouldRenderFace  = Liquid_shouldRenderFace;

    TILE_WATER.renderFace      = Liquid_renderFace;
    TILE_CALM_WATER.renderFace = Liquid_renderFace;
    TILE_LAVA.renderFace       = Liquid_renderFace;
    TILE_CALM_LAVA.renderFace  = Liquid_renderFace;

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

    registerTile(&TILE_SAND,       12, 18, NULL);
    registerTile(&TILE_GRAVEL,     13, 19, NULL);
    registerTile(&TILE_GOLD_ORE,   14, 32, NULL);
    registerTile(&TILE_IRON_ORE,   15, 33, NULL);
    registerTile(&TILE_COAL_ORE,   16, 34, NULL);
    // c0.0.23a_01: textureId (used for break particles, see Particle_init)
    // is 20, not 0. Confirmed directly against the real source's Log
    // subclass, which explicitly sets its base texture field to 20 (the bark
    // texture) in its constructor, on top of its own per face getTexture
    // override; Grass's equivalent field was already correct (3)
    registerTile(&TILE_LOG,        17, 20, Log_getTexture);
    registerTile(&TILE_LEAVES,     18, 22, NULL);

    TILE_SAND.hardnessTicks     = 10;
    TILE_GRAVEL.hardnessTicks   = 12;
    TILE_GOLD_ORE.hardnessTicks = 60;
    TILE_IRON_ORE.hardnessTicks = 60;
    TILE_COAL_ORE.hardnessTicks = 60;
    TILE_LOG.hardnessTicks      = 50;
    TILE_LEAVES.hardnessTicks   = 4;

    TILE_SAND.onTick   = TILE_GRAVEL.onTick   = FallingTile_onTick;
    TILE_SAND.neighborChanged = TILE_GRAVEL.neighborChanged = FallingTile_neighborChanged;

    TILE_LEAVES.isSolid     = Leaves_isSolid;
    TILE_LEAVES.blocksLight = Leaves_blocksLight;
    TILE_LEAVES.particleGravity = 0.4f; // c0.0.16a_02: leaf break particles fall slower

    // c0.24_st_03: survival tile drops (task #60's generic plumbing, real
    // per-tile values from level\tile\d.java/e.java)
    TILE_LOG.getDropCount       = Log_getDropCount;
    TILE_LOG.getDropResource    = Log_getDropResource;
    TILE_LEAVES.getDropCount    = Leaves_getDropCount;
    TILE_LEAVES.getDropResource = Leaves_getDropResource;

    TILE_SAND.soundType     = SOUND_GRAVEL;
    TILE_GRAVEL.soundType   = SOUND_GRAVEL;
    TILE_GOLD_ORE.soundType = SOUND_STONE;
    TILE_IRON_ORE.soundType = SOUND_STONE;
    TILE_COAL_ORE.soundType = SOUND_STONE;
    TILE_LOG.soundType      = SOUND_WOOD;
    // c0.0.23a_01: Leaves uses SOUND_GRASS, matching the real source's own
    // per tile constructor call exactly (not SOUND_NONE, despite Leaves being
    // otherwise a fairly "insubstantial" non solid, non light blocking tile)
    TILE_LEAVES.soundType   = SOUND_GRASS;

    // c0.0.19a_04: texture 48 is inferred, not directly present in the
    // decompiled bytecode (the constructor call never assigns one there).
    // terrain.png's atlas cell 48 visibly contains a sponge texture unused
    // by anything else, so this is treated as a decompilation artifact
    // rather than a real "texture 0" bug
    registerTile(&TILE_SPONGE, 19, 48, NULL);
    registerTile(&TILE_GLASS,  20, 49, NULL);

    TILE_SPONGE.hardnessTicks = 12;
    TILE_GLASS.hardnessTicks  = 6;

    TILE_SPONGE.onPlace   = Sponge_onPlace;
    TILE_SPONGE.onRemoved = Sponge_onRemoved;
    TILE_SPONGE.soundType = SOUND_CLOTH;
    // c0.0.23a_01: read directly from the real source's own constructor call
    // (0.9, not the default 1.0), the only other tile besides Leaves with a
    // non default particle gravity
    TILE_SPONGE.particleGravity = 0.9f;

    TILE_GLASS.blocksLight     = Glass_blocksLight;
    TILE_GLASS.isSolid         = Glass_isSolid;
    TILE_GLASS.shouldRenderFace = Glass_shouldRenderFace;
    // c0.0.23a_01: confirmed real quirk, glass makes a metallic clink, not a
    // glass shatter sound, in the original game
    TILE_GLASS.soundType       = SOUND_METAL;

    // c0.0.20a_02: 16 Cloth colors, one full terrain.png row, placeable only,
    // no special behavior beyond texture id
    for (int i = 0; i < 16; ++i) {
        registerTile(&TILE_CLOTH[i], 21 + i, 64 + i, NULL);
        TILE_CLOTH[i].soundType = SOUND_CLOTH;
        TILE_CLOTH[i].hardnessTicks = 16;
    }

    // c0.0.20a_02: Dandelion/Rose/Mushrooms reuse Bush's tile class outright,
    // placeable only, no world generation spawn
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
        plants[i]->onTick      = Bush_onTick;
        plants[i]->hardnessTicks = 0; // instant, matches Bush
    }

    // c0.0.20a_02: Gold Block, plain tile, no special behavior
    registerTile(&TILE_GOLD_BLOCK, 41, 40, NULL);
    TILE_GOLD_BLOCK.soundType = SOUND_METAL;
    TILE_GOLD_BLOCK.hardnessTicks = 60;
}

/* untextured single face helper for hit highlight */
// only draws a face if the player is on the side it faces, matching the real
// game's block highlight (was drawing all 6 faces unconditionally)
void Face_render(Tessellator* t, int x, int y, int z, int face, float px, float py, float pz) {
    const float minX = (float)x,     maxX = (float)x + 1.0f;
    const float minY = (float)y,     maxY = (float)y + 1.0f;
    const float minZ = (float)z,     maxZ = (float)z + 1.0f;

    if (face == 0 && y > py) { // bottom
        Tessellator_vertex(t, minX, minY, maxZ);
        Tessellator_vertex(t, minX, minY, minZ);
        Tessellator_vertex(t, maxX, minY, minZ);
        Tessellator_vertex(t, maxX, minY, maxZ);
    } else if (face == 1 && y < py) { // top
        Tessellator_vertex(t, maxX, maxY, maxZ);
        Tessellator_vertex(t, maxX, maxY, minZ);
        Tessellator_vertex(t, minX, maxY, minZ);
        Tessellator_vertex(t, minX, maxY, maxZ);
    } else if (face == 2 && z > pz) { // negative Z
        Tessellator_vertex(t, minX, maxY, minZ);
        Tessellator_vertex(t, maxX, maxY, minZ);
        Tessellator_vertex(t, maxX, minY, minZ);
        Tessellator_vertex(t, minX, minY, minZ);
    } else if (face == 3 && z < pz) { // positive Z
        Tessellator_vertex(t, minX, maxY, maxZ);
        Tessellator_vertex(t, minX, minY, maxZ);
        Tessellator_vertex(t, maxX, minY, maxZ);
        Tessellator_vertex(t, maxX, maxY, maxZ);
    } else if (face == 4 && x > px) { // negative X
        Tessellator_vertex(t, minX, maxY, maxZ);
        Tessellator_vertex(t, minX, maxY, minZ);
        Tessellator_vertex(t, minX, minY, minZ);
        Tessellator_vertex(t, minX, minY, maxZ);
    } else if (face == 5 && x < px) { // +X
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
        Particle_init(&p, lvl, tx, ty, tz, mx, my, mz, self);
        ParticleEngine_add(engine, &p);
    }
}

// implemented in minecraft.c: allocates a slot in items[] and spawns it.
// tile.c has no access to that array (lives with the other entity arrays in
// minecraft.c, this project's usual pattern for that layering problem, see
// the equivalent for Arrow's own entity search in item/arrow.c)
extern void Minecraft_spawnItem(Level* lvl, float x, float y, float z, int resource);

void Tile_dropItems(const Tile* self, Level* lvl, int x, int y, int z) {
    int count = self->getDropCount(self);
    int resource = self->getDropResource(self);
    for (int i = 0; i < count; i++) {
        float jx = (float)rand() / (float)RAND_MAX * 0.7f + 0.15f;
        float jy = (float)rand() / (float)RAND_MAX * 0.7f + 0.15f;
        float jz = (float)rand() / (float)RAND_MAX * 0.7f + 0.15f;
        Minecraft_spawnItem(lvl, (float)x + jx, (float)y + jy, (float)z + jz, resource);
    }
}