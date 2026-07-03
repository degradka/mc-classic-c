// tile.c: registry, per face textures, render helpers

#include "tile.h"
#include "../../particle/particle_engine.h"
#include "../../particle/particle.h"
#include <string.h>

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

static inline int shouldRenderFace(const Level* lvl, int x, int y, int z, int layer) {
    // match Java logic: face visible if not solid AND (isLit XOR (layer==1))
    const int lit = (x < 0 || y < 0 || z < 0 || x >= lvl->width || y >= lvl->depth || z >= lvl->height)
                  ? 1
                  : (y >= lvl->lightDepths[x + z * lvl->width]);
    return (!Level_isSolidTile(lvl, x, y, z)) && ((lit ^ (layer == 1)) != 0);
}

// helper to compute UVs from atlas slot (16x16 tiles on 256x256)
static void calcUV(int slot, float* u0, float* v0, float* u1, float* v1) {
    // 16x16 tiles in a 256x256 atlas
    float minU = (slot % 16) / 16.0f;
    float minV = (slot / 16) / 16.0f;
    *u0 = minU;
    *v0 = minV;
    *u1 = minU + 16.0f / 256.0f;
    *v1 = minV + 16.0f / 256.0f;
}

static void Tile_render_shared(const Tile* self, Tessellator* t, const Level* lvl, int layer, int x, int y, int z) {
    const float shadeX = 0.6f, shadeY = 1.0f, shadeZ = 0.8f;

    const float minX = (float)x, maxX = (float)x + 1.0f;
    const float minY = (float)y, maxY = (float)y + 1.0f;
    const float minZ = (float)z, maxZ = (float)z + 1.0f;

    float u0, v0, u1, v1;

    // bottom (face 0)
    if (shouldRenderFace(lvl, x, y - 1, z, layer)) {
        Tessellator_color(t, 1.0f, 1.0f, 1.0f);
        calcUV(self->getTexture(self, 0), &u0,&v0,&u1,&v1);
        Tessellator_vertexUV(t, minX, minY, maxZ, u0, v1);
        Tessellator_vertexUV(t, minX, minY, minZ, u0, v0);
        Tessellator_vertexUV(t, maxX, minY, minZ, u1, v0);
        Tessellator_vertexUV(t, maxX, minY, maxZ, u1, v1);
    }

    // top (face 1)
    if (shouldRenderFace(lvl, x, y + 1, z, layer)) {
        Tessellator_color(t, shadeY, shadeY, shadeY);
        calcUV(self->getTexture(self, 1), &u0, &v0, &u1, &v1);
        Tessellator_vertexUV(t, maxX, maxY, maxZ, u1, v1);
        Tessellator_vertexUV(t, maxX, maxY, minZ, u1, v0);
        Tessellator_vertexUV(t, minX, maxY, minZ, u0, v0);
        Tessellator_vertexUV(t, minX, maxY, maxZ, u0, v1);
    }

    // negative Z (face 2)
    if (shouldRenderFace(lvl, x, y, z - 1, layer)) {
        Tessellator_color(t, shadeZ, shadeZ, shadeZ);
        calcUV(self->getTexture(self, 2), &u0, &v0, &u1, &v1);
        Tessellator_vertexUV(t, minX, maxY, minZ, u1, v0);
        Tessellator_vertexUV(t, maxX, maxY, minZ, u0, v0);
        Tessellator_vertexUV(t, maxX, minY, minZ, u0, v1);
        Tessellator_vertexUV(t, minX, minY, minZ, u1, v1);
    }

    // +Z (face 3)
    if (shouldRenderFace(lvl, x, y, z + 1, layer)) {
        Tessellator_color(t, shadeZ, shadeZ, shadeZ);
        calcUV(self->getTexture(self, 3), &u0, &v0, &u1, &v1);
        Tessellator_vertexUV(t, minX, maxY, maxZ, u0, v0);
        Tessellator_vertexUV(t, minX, minY, maxZ, u0, v1);
        Tessellator_vertexUV(t, maxX, minY, maxZ, u1, v1);
        Tessellator_vertexUV(t, maxX, maxY, maxZ, u1, v0);
    }

    // negative X (face 4)
    if (shouldRenderFace(lvl, x - 1, y, z, layer)) {
        Tessellator_color(t, shadeX, shadeX, shadeX);
        calcUV(self->getTexture(self, 4), &u0, &v0, &u1, &v1);
        Tessellator_vertexUV(t, minX, maxY, maxZ, u1, v0);
        Tessellator_vertexUV(t, minX, maxY, minZ, u0, v0);
        Tessellator_vertexUV(t, minX, minY, minZ, u0, v1);
        Tessellator_vertexUV(t, minX, minY, maxZ, u1, v1);
    }

    // +X (face 5)
    if (shouldRenderFace(lvl, x + 1, y, z, layer)) {
        Tessellator_color(t, shadeX, shadeX, shadeX);
        calcUV(self->getTexture(self, 5), &u0, &v0, &u1, &v1);
        Tessellator_vertexUV(t, maxX, minY, maxZ, u0, v1);
        Tessellator_vertexUV(t, maxX, minY, minZ, u1, v1);
        Tessellator_vertexUV(t, maxX, maxY, minZ, u1, v0);
        Tessellator_vertexUV(t, maxX, maxY, maxZ, u0, v0);
    }
}

/* tile instances and registry */

const Tile* gTiles[256] = { 0 };

static void Tile_default_neighborChanged(const Tile* self, Level* lvl, int x, int y, int z, int type) {
    (void)self; (void)lvl; (void)x; (void)y; (void)z; (void)type;
}

static void registerTile(Tile* t, int id, int tex, int (*getTex)(const Tile*,int)) {
    t->id = id; t->textureId = tex;
    t->liquidType = LIQUID_NONE;
    t->tileId = t->calmTileId = t->spreadSpeed = 0;
    t->getTexture = getTex ? getTex : Tile_default_getTexture;
    t->render = Tile_render_shared;
    t->onTick = NULL;
    t->isSolid     = Tile_default_isSolid;
    t->blocksLight = Tile_default_blocksLight;
    t->getAABB     = Tile_default_getAABB;
    t->neighborChanged = Tile_default_neighborChanged;

    gTiles[id] = t;
}

Tile TILE_ROCK;
Tile TILE_DIRT;
Tile TILE_STONEBRICK;
Tile TILE_WOOD;
Tile TILE_GRASS;
Tile TILE_BUSH;
Tile TILE_WATER;
Tile TILE_CALM_WATER;
Tile TILE_LAVA;
Tile TILE_CALM_LAVA;

/* Grass has per face textures: top is 0, bottom is 2, sides are 3 */
static int Grass_getTexture(const Tile* self, int face) {
    (void)self;
    return (face == 1) ? 0 : (face == 0) ? 2 : 3;
}

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

static int Liquid_checkWater(const Tile* self, Level* lvl, int x, int y, int z, int depth);

// Falls straight down through air, then spreads to the 4 side neighbors.
// Lava only ever falls one step per tick. Water keeps falling as long as
// it keeps succeeding. This asymmetry is in the original source.
static int Liquid_updateWater(const Tile* self, Level* lvl, int x, int y, int z, int depth) {
    int hasChanged = 0;

    while (Level_getTile(lvl, x, --y, z) == 0) {
        int change = level_setTile(lvl, x, y, z, self->tileId);
        if (change) hasChanged = 1;
        if (!change || self->liquidType == LIQUID_LAVA) break;
    }
    y++;

    if (self->liquidType == LIQUID_WATER || !hasChanged) {
        // deliberately not short circuited, all 4 neighbors must always be checked
        int c1 = Liquid_checkWater(self, lvl, x - 1, y, z, depth);
        int c2 = Liquid_checkWater(self, lvl, x + 1, y, z, depth);
        int c3 = Liquid_checkWater(self, lvl, x, y, z - 1, depth);
        int c4 = Liquid_checkWater(self, lvl, x, y, z + 1, depth);
        hasChanged = hasChanged || c1 || c2 || c3 || c4;
    }

    if (!hasChanged) {
        Level_setTileNoUpdate(lvl, x, y, z, self->calmTileId);
    }
    return hasChanged;
}

static int Liquid_checkWater(const Tile* self, Level* lvl, int x, int y, int z, int depth) {
    if (Level_getTile(lvl, x, y, z) != 0) return 0;

    int changed = level_setTile(lvl, x, y, z, self->tileId);
    if (changed && depth < self->spreadSpeed) {
        return Liquid_updateWater(self, lvl, x, y, z, depth + 1);
    }
    return 0;
}

static void Liquid_tick(const Tile* self, Level* lvl, int x, int y, int z) {
    Liquid_updateWater(self, lvl, x, y, z, 0);
}

static void Liquid_neighborChanged(const Tile* self, Level* lvl, int x, int y, int z, int type) {
    if (self->liquidType == LIQUID_WATER && (type == TILE_LAVA.id || type == TILE_CALM_LAVA.id)) {
        Level_setTileNoUpdate(lvl, x, y, z, TILE_ROCK.id);
    }
    if (self->liquidType == LIQUID_LAVA && (type == TILE_WATER.id || type == TILE_CALM_WATER.id)) {
        Level_setTileNoUpdate(lvl, x, y, z, TILE_ROCK.id);
    }
}

static void CalmLiquid_neighborChanged(const Tile* self, Level* lvl, int x, int y, int z, int type) {
    int hasAirNeighbor = 0;
    if (Level_getTile(lvl, x - 1, y, z) == 0) hasAirNeighbor = 1;
    if (Level_getTile(lvl, x + 1, y, z) == 0) hasAirNeighbor = 1;
    if (Level_getTile(lvl, x, y, z - 1) == 0) hasAirNeighbor = 1;
    if (Level_getTile(lvl, x, y, z + 1) == 0) hasAirNeighbor = 1;
    if (Level_getTile(lvl, x, y - 1, z) == 0) hasAirNeighbor = 1;

    if (hasAirNeighbor) {
        Level_setTileNoUpdate(lvl, x, y, z, self->tileId); // start flowing again
    }
    if (self->liquidType == LIQUID_WATER && type == TILE_LAVA.id) {
        Level_setTileNoUpdate(lvl, x, y, z, TILE_ROCK.id);
    }
    if (self->liquidType == LIQUID_LAVA && type == TILE_WATER.id) {
        Level_setTileNoUpdate(lvl, x, y, z, TILE_ROCK.id);
    }
}

static void Bush_render(const Tile* self, Tessellator* t, const Level* lvl,
                        int layer, int x, int y, int z)
{
    // Visibility rule: render in exactly one of the two layers (lit xor shadow),
    // like the rest of tiles. Use the same lit test the shared renderer uses.
    const int lit = (x < 0 || y < 0 || z < 0 || x >= lvl->width || y >= lvl->depth || z >= lvl->height)
                    ? 1
                    : (y >= lvl->lightDepths[x + z * lvl->width]);
    if ( ((lit ^ (layer == 1)) == 0) ) return;

    // Bush uses atlas slot 15 in this commit
    float u0, v0, u1, v1;
    calcUV(self->textureId, &u0,&v0,&u1,&v1);

    // Slight shading like leaves; you can also just use 1,1,1
    const float shade = 0.8f;
    Tessellator_color(t, shade, shade, shade);

    const float X0 = (float)x, X1 = (float)x + 1.0f;
    const float Y0 = (float)y, Y1 = (float)y + 1.0f;
    const float Z0 = (float)z, Z1 = (float)z + 1.0f;

    // We draw both sides of each diagonal plane (so it's visible with culling on).
    // Plane A: from (x,*,z) to (x+1,*,z+1)  (diagonal ↘)
    //   front
    Tessellator_vertexUV(t, X0, Y1, Z0, u0, v0);
    Tessellator_vertexUV(t, X1, Y1, Z1, u1, v0);
    Tessellator_vertexUV(t, X1, Y0, Z1, u1, v1);
    Tessellator_vertexUV(t, X0, Y0, Z0, u0, v1);
    //   back
    Tessellator_vertexUV(t, X0, Y1, Z0, u0, v0);
    Tessellator_vertexUV(t, X0, Y0, Z0, u0, v1);
    Tessellator_vertexUV(t, X1, Y0, Z1, u1, v1);
    Tessellator_vertexUV(t, X1, Y1, Z1, u1, v0);

    // Plane B: from (x+1,*,z) to (x,*,z+1)   (diagonal ↙)
    //   front
    Tessellator_vertexUV(t, X1, Y1, Z0, u0, v0);
    Tessellator_vertexUV(t, X0, Y1, Z1, u1, v0);
    Tessellator_vertexUV(t, X0, Y0, Z1, u1, v1);
    Tessellator_vertexUV(t, X1, Y0, Z0, u0, v1);
    //   back
    Tessellator_vertexUV(t, X1, Y1, Z0, u0, v0);
    Tessellator_vertexUV(t, X1, Y0, Z0, u0, v1);
    Tessellator_vertexUV(t, X0, Y0, Z1, u1, v1);
    Tessellator_vertexUV(t, X0, Y1, Z1, u1, v0);
}

static void Bush_onTick(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    int below = Level_getTile(lvl, x, y-1, z);
    if (!Level_isLit(lvl, x, y, z) || (below != TILE_DIRT.id && below != TILE_GRASS.id)) {
        level_setTile(lvl, x, y, z, 0);
    }
}

void Tile_registerAll(void) {
    memset((void*)gTiles, 0, sizeof(gTiles));
    registerTile(&TILE_ROCK,       1,  1,  NULL);
    registerTile(&TILE_GRASS,      2,  3,  Grass_getTexture);
    registerTile(&TILE_DIRT,       3,  2,  NULL);
    registerTile(&TILE_STONEBRICK, 4, 16,  NULL);
    registerTile(&TILE_WOOD,       5,  4,  NULL);
    registerTile(&TILE_BUSH,       6, 15,  NULL);

    TILE_GRASS.onTick     = Grass_onTick;

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

    // flowing variants: tileId==own id, calmTileId==the paired calm id
    TILE_WATER.tileId = TILE_WATER.id; TILE_WATER.calmTileId = TILE_CALM_WATER.id; TILE_WATER.spreadSpeed = 8;
    TILE_LAVA.tileId  = TILE_LAVA.id;  TILE_LAVA.calmTileId  = TILE_CALM_LAVA.id;  TILE_LAVA.spreadSpeed  = 2;
    // calm variants share the same flowing/calm id pair as their flowing counterpart
    TILE_CALM_WATER.tileId = TILE_WATER.id; TILE_CALM_WATER.calmTileId = TILE_CALM_WATER.id; TILE_CALM_WATER.spreadSpeed = 8;
    TILE_CALM_LAVA.tileId  = TILE_LAVA.id;  TILE_CALM_LAVA.calmTileId  = TILE_CALM_LAVA.id;  TILE_CALM_LAVA.spreadSpeed  = 2;

    TILE_WATER.onTick = Liquid_tick;
    TILE_LAVA.onTick  = Liquid_tick;
    // calm variants don't tick (setTicking(false) in the original)

    TILE_WATER.neighborChanged      = Liquid_neighborChanged;
    TILE_LAVA.neighborChanged       = Liquid_neighborChanged;
    TILE_CALM_WATER.neighborChanged = CalmLiquid_neighborChanged;
    TILE_CALM_LAVA.neighborChanged  = CalmLiquid_neighborChanged;
}

/* untextured single face helper for hit highlight */
void Face_render(Tessellator* t, int x, int y, int z, int face) {
    const float minX = (float)x,     maxX = (float)x + 1.0f;
    const float minY = (float)y,     maxY = (float)y + 1.0f;
    const float minZ = (float)z,     maxZ = (float)z + 1.0f;

    if (face == 0) { // bottom
        Tessellator_vertex(t, minX, minY, maxZ);
        Tessellator_vertex(t, minX, minY, minZ);
        Tessellator_vertex(t, maxX, minY, minZ);
        Tessellator_vertex(t, maxX, minY, maxZ);
    } else if (face == 1) { // top
        Tessellator_vertex(t, maxX, maxY, maxZ);
        Tessellator_vertex(t, maxX, maxY, minZ);
        Tessellator_vertex(t, minX, maxY, minZ);
        Tessellator_vertex(t, minX, maxY, maxZ);
    } else if (face == 2) { // negative Z
        Tessellator_vertex(t, minX, maxY, minZ);
        Tessellator_vertex(t, maxX, maxY, minZ);
        Tessellator_vertex(t, maxX, minY, minZ);
        Tessellator_vertex(t, minX, minY, minZ);
    } else if (face == 3) { // positive Z
        Tessellator_vertex(t, minX, maxY, maxZ);
        Tessellator_vertex(t, minX, minY, maxZ);
        Tessellator_vertex(t, maxX, minY, maxZ);
        Tessellator_vertex(t, maxX, maxY, maxZ);
    } else if (face == 4) { // negative X
        Tessellator_vertex(t, minX, maxY, maxZ);
        Tessellator_vertex(t, minX, maxY, minZ);
        Tessellator_vertex(t, minX, minY, minZ);
        Tessellator_vertex(t, minX, minY, maxZ);
    } else if (face == 5) { // +X
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
        Particle_init(&p, lvl, tx, ty, tz, mx, my, mz, self->textureId);
        ParticleEngine_add(engine, &p);
    }
}