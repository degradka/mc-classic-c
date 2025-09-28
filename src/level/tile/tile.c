// level/tile/tile.c — registry, per-face textures, render helpers

#include "tile.h"
#include "liquid_tile.h"
#include "../../particle/particle_engine.h"
#include "../../particle/particle.h"
#include <string.h>

static int  Tile_default_getLiquidType(const Tile* self){ (void)self; return LIQ_NONE; }
static void Tile_default_neighborChanged(const Tile* self, Level* lvl, int x,int y,int z,int type){
    (void)self; (void)lvl; (void)x; (void)y; (void)z; (void)type;
}

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
    float xt = (slot % 16) * 16.0f;
    float yt = (slot / 16) * 16.0f;
    *u0 = (xt + 0.01f)   / 256.0f;
    *v0 = (yt + 0.01f)   / 256.0f;
    *u1 = (xt + 15.99f)  / 256.0f;
    *v1 = (yt + 15.99f)  / 256.0f;
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

    // -Z (face 2)
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

    // -X (face 4)
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

/* ---------- tile instances & registry ---------- */

const Tile* gTiles[256] = { 0 };

static void registerTile(Tile* t, int id, int tex, int (*getTex)(const Tile*,int)) {
    t->id = id; t->textureId = tex;
    t->getTexture = getTex ? getTex : Tile_default_getTexture;
    t->render = Tile_render_shared;
    t->onTick = NULL;
    t->isSolid     = Tile_default_isSolid;
    t->blocksLight = Tile_default_blocksLight;
    t->getAABB     = Tile_default_getAABB;
    t->neighborChanged = Tile_default_neighborChanged;
    t->getLiquidType   = Tile_default_getLiquidType;

    gTiles[id] = t;
}

Tile TILE_ROCK;
Tile TILE_DIRT;
Tile TILE_STONEBRICK;
Tile TILE_WOOD;
Tile TILE_GRASS;
Tile TILE_BUSH;

Tile TILE_UNBREAKABLE;
LiquidTile TILE_WATER;
LiquidTile TILE_CALM_WATER;
LiquidTile TILE_LAVA;
LiquidTile TILE_CALM_LAVA;

/* Grass (per-face textures) — face map: top=0, bottom=2, sides=3 */
static int Grass_getTexture(const Tile* self, int face) {
    (void)self;
    return (face == 1) ? 0 : (face == 0) ? 2 : 3;
}

static void Grass_onTick(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;

    // 25% chance to do any work this tick
    if ((rand() & 3) != 0) return;

    // Decay if no light above
    if (!Level_isLit(lvl, x, y + 1, z)) {
        Level_setTile(lvl, x, y, z, TILE_DIRT.id);
        return;
    }

    // Try to spread to nearby dirt that has light above it
    for (int i = 0; i < 4; ++i) {
        int xt = x + (rand() % 3) - 1;   // [-1..+1]
        int yt = y + (rand() % 5) - 3;   // [-3..+1]
        int zt = z + (rand() % 3) - 1;   // [-1..+1]
        if (Level_getTile(lvl, xt, yt, zt) == TILE_DIRT.id &&
            Level_isLit(lvl, xt, yt + 1, zt)) {
            Level_setTile(lvl, xt, yt, zt, TILE_GRASS.id);
        }
    }
}

static int Bush_isSolid(const Tile* self)     { (void)self; return 0; }
static int Bush_blocksLight(const Tile* self) { (void)self; return 0; }
static int Bush_getAABB(const Tile* self, int x,int y,int z, AABB* out){
    (void)self; (void)x; (void)y; (void)z; (void)out;
    return 0; // no collision box
}

static void Bush_render(const Tile* self, Tessellator* t, const Level* lvl,
                        int layer, int x, int y, int z)
{
    // Only render in the matching light layer (lit XOR (layer==1))
    const int lit = (x < 0 || y < 0 || z < 0 || x >= lvl->width || y >= lvl->depth || z >= lvl->height)
                    ? 1
                    : (y >= lvl->lightDepths[x + z * lvl->width]);
    if ( ((lit ^ (layer == 1)) == 0) ) return;

    float u0,v0,u1,v1;
    calcUV(self->textureId, &u0,&v0,&u1,&v1);

    // 0.0.13a uses white (byte) color
    Tessellator_colorBytes(t, 255, 255, 255);

    const int rots = 2;
    const float cx = x + 0.5f;
    const float cz = z + 0.5f;
    const float y0 = (float)y;
    const float y1 = (float)y + 1.0f;

    for (int r = 0; r < rots; ++r) {
        float ang = (float)(r * M_PI / rots + 0.7853981633974483); // +45°
        float xa = (float)(sin(ang) * 0.5);
        float za = (float)(cos(ang) * 0.5);

        float x0 = cx - xa, x1 = cx + xa;
        float z0 = cz - za, z1 = cz + za;

        // Front
        Tessellator_vertexUV(t, x0, y1, z0, u1, v0);
        Tessellator_vertexUV(t, x1, y1, z1, u0, v0);
        Tessellator_vertexUV(t, x1, y0, z1, u0, v1);
        Tessellator_vertexUV(t, x0, y0, z0, u1, v1);

        // Back (note UVs match 0.0.13a)
        Tessellator_vertexUV(t, x1, y1, z1, u1, v0);
        Tessellator_vertexUV(t, x0, y1, z0, u0, v0);
        Tessellator_vertexUV(t, x0, y0, z0, u0, v1);
        Tessellator_vertexUV(t, x1, y0, z1, u1, v1);
    }
}

static void Bush_onTick(const Tile* self, Level* lvl, int x, int y, int z) {
    (void)self;
    int below = Level_getTile(lvl, x, y-1, z);
    if (!Level_isLit(lvl, x, y, z) || (below != TILE_DIRT.id && below != TILE_GRASS.id)) {
        Level_setTile(lvl, x, y, z, 0);
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

    // id 7: unbreakable (solid/blocksLight, cube AABB)
    registerTile(&TILE_UNBREAKABLE, 7, 17, NULL);

    // liquids: 8..11
    LiquidTile_init(&TILE_WATER,       8, LIQ_WATER, 0);
    LiquidTile_init(&TILE_CALM_WATER,  9, LIQ_WATER, 1);
    LiquidTile_init(&TILE_LAVA,       10, LIQ_LAVA,  0);
    LiquidTile_init(&TILE_CALM_LAVA,  11, LIQ_LAVA,  1);
    gTiles[8]  = &TILE_WATER.base;
    gTiles[9]  = &TILE_CALM_WATER.base;
    gTiles[10] = &TILE_LAVA.base;
    gTiles[11] = &TILE_CALM_LAVA.base;

    // existing grass/bush overrides
    TILE_GRASS.onTick     = Grass_onTick;

    TILE_BUSH.isSolid     = Bush_isSolid;
    TILE_BUSH.blocksLight = Bush_blocksLight;
    TILE_BUSH.getAABB     = Bush_getAABB;
    TILE_BUSH.render      = Bush_render;
    TILE_BUSH.onTick      = Bush_onTick;
}

/* ---------- untextured single-face helper (for hit highlight) ---------- */
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
    } else if (face == 2) { // -Z
        Tessellator_vertex(t, minX, maxY, minZ);
        Tessellator_vertex(t, maxX, maxY, minZ);
        Tessellator_vertex(t, maxX, minY, minZ);
        Tessellator_vertex(t, minX, minY, minZ);
    } else if (face == 3) { // +Z
        Tessellator_vertex(t, minX, maxY, maxZ);
        Tessellator_vertex(t, minX, minY, maxZ);
        Tessellator_vertex(t, maxX, minY, maxZ);
        Tessellator_vertex(t, maxX, maxY, maxZ);
    } else if (face == 4) { // -X
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