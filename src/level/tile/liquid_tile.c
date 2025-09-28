// level/tile/liquid_tile.c

#include "liquid_tile.h"
#include "../level.h"
#include "../level_renderer.h"
#include "../../renderer/tessellator.h"
#include <stdlib.h>
#include <GL/glew.h>

/* ---- helpers ------------------------------------------------------------ */

static void calcUV_liquid(int slot, float* u0, float* v0, float* u1, float* v1) {
    float xt = (slot % 16) * 16.0f;
    float yt = (slot / 16) * 16.0f;
    *u0 = xt / 256.0f;
    *v0 = yt / 256.0f;
    *u1 = (xt + 15.99f) / 256.0f;
    *v1 = (yt + 15.99f) / 256.0f;
}

static int Liquid_mayPick(const Tile* self) { (void)self; return 0; }

static void Liquid_emit_face_both(Tessellator* t,
                                  float ax,float ay,float az, float bx,float by,float bz,
                                  float cx,float cy,float cz, float dx,float dy,float dz,
                                  float u0,float v0,float u1,float v1)
{
    // front (matches Tile.renderFace)
    Tessellator_vertexUV(t, ax, ay, az, u0, v1);
    Tessellator_vertexUV(t, bx, by, bz, u0, v0);
    Tessellator_vertexUV(t, cx, cy, cz, u1, v0);
    Tessellator_vertexUV(t, dx, dy, dz, u1, v1);

    // back (matches Tile.renderBackFace)
    Tessellator_vertexUV(t, dx, dy, dz, u0, v1);
    Tessellator_vertexUV(t, cx, cy, cz, u0, v0);
    Tessellator_vertexUV(t, bx, by, bz, u1, v0);
    Tessellator_vertexUV(t, ax, ay, az, u1, v1);
}

static int liquid_shouldRenderFace(const LiquidTile* lt, const Level* lvl, int nx, int ny, int nz, int layer) {
    // bounds: if out of bounds, we don't render faces (matches Java)
    if (nx < 0 || ny < 0 || nz < 0 || nx >= lvl->width || ny >= lvl->depth || nz >= lvl->height) return 0;

    if (lt->liquidType == LIQ_WATER && layer != 2) return 0; // water only in translucent layer

    int id = Level_getTile(lvl, nx, ny, nz);
    if (id == lt->tileId || id == lt->calmTileId) return 0; // don't draw against same liquid

    // Defer to base rule that ignores light XOR: test visibility against air/non-solid
    return (!Level_isSolidTile(lvl, nx, ny, nz));
}

/* ---- behavior ----------------------------------------------------------- */

static int Liquid_getTexture(const Tile* self, int face) {
    (void)face;
    // water uses tex 14, lava 30
    const LiquidTile* lt = (const LiquidTile*)self;
    return (lt->liquidType == LIQ_LAVA) ? 30 : 14;
}

static void Liquid_render(const Tile* self, Tessellator* t, const Level* lvl, int layer, int x, int y, int z) {
    const LiquidTile* lt = (const LiquidTile*)self;

    if (lt->liquidType == LIQ_WATER && layer != 2) return;

    int tex = self->getTexture(self, 0);
    float u0,v0,u1,v1; calcUV_liquid(tex, &u0,&v0,&u1,&v1);

    const float x0 = x + self->x0, x1 = x + self->x1;
    const float y0 = y + self->y0, y1 = y + self->y1;
    const float z0 = z + self->z0, z1 = z + self->z1;

    Tessellator_color(t, 1.0f, 1.0f, 1.0f);

    // bottom (face 0)
    if (liquid_shouldRenderFace(lt, lvl, x, y-1, z, layer)) {
        Liquid_emit_face_both(t,
            x0,y0,z1,  x0,y0,z0,  x1,y0,z0,  x1,y0,z1,
            u0,v0,u1,v1);
    }

    // top (1)
    if (liquid_shouldRenderFace(lt, lvl, x, y+1, z, layer)) {
        Liquid_emit_face_both(t,
            x1,y1,z1,  x1,y1,z0,  x0,y1,z0,  x0,y1,z1,
            u0,v0,u1,v1);
    }

    // -Z (2)
    if (liquid_shouldRenderFace(lt, lvl, x, y, z-1, layer)) {
        Liquid_emit_face_both(t,
            x0,y1,z0,  x1,y1,z0,  x1,y0,z0,  x0,y0,z0,
            u0,v0,u1,v1);
    }

    // +Z (3)
    if (liquid_shouldRenderFace(lt, lvl, x, y, z+1, layer)) {
        Liquid_emit_face_both(t,
            x0,y1,z1,  x0,y0,z1,  x1,y0,z1,  x1,y1,z1,
            u0,v0,u1,v1);
    }

    // -X (4)
    if (liquid_shouldRenderFace(lt, lvl, x-1, y, z, layer)) {
        Liquid_emit_face_both(t,
            x0,y1,z1,  x0,y1,z0,  x0,y0,z0,  x0,y0,z1,
            u0,v0,u1,v1);
    }

    // +X (5)
    if (liquid_shouldRenderFace(lt, lvl, x+1, y, z, layer)) {
        Liquid_emit_face_both(t,
            x1,y0,z1,  x1,y0,z0,  x1,y1,z0,  x1,y1,z1,
            u0,v0,u1,v1);
    }
}

// Flowing update

static int Liquid_checkWater(LiquidTile* lt, Level* lvl, int x, int y, int z, int depth);

static void Liquid_tick(const Tile* self, Level* lvl, int x, int y, int z) {
    LiquidTile* lt = (LiquidTile*)self;

    // Java does: updateWater(level, x,y,z, 0)
    int hasChanged = 0;

    // fall straight down while air
    int yy = y;
    while (Level_getTile(lvl, x, yy-1, z) == 0) {
        int changed = Level_setTile(lvl, x, yy-1, z, lt->tileId);
        if (changed) hasChanged = 1;
        if (!changed || lt->liquidType == LIQ_LAVA) break; // lava doesn't keep falling recursively
        yy--;
    }

    // If water or nothing changed falling, try spreading horizontally
    if (lt->liquidType == LIQ_WATER || !hasChanged) {
        hasChanged |= Liquid_checkWater(lt, lvl, x-1, y, z, 0);
        hasChanged |= Liquid_checkWater(lt, lvl, x+1, y, z, 0);
        hasChanged |= Liquid_checkWater(lt, lvl, x, y, z-1, 0);
        hasChanged |= Liquid_checkWater(lt, lvl, x, y, z+1, 0);
    }

    if (!hasChanged) {
        // turn into calm
        Level_setTileNoUpdate(lvl, x, y, z, lt->calmTileId);
        if (lvl->renderer) LevelRenderer_tileChanged(lvl->renderer, x, y, z);
    }
}

static int Liquid_checkWater(LiquidTile* lt, Level* lvl, int x, int y, int z, int depth) {
    int hasChanged = 0;
    int type = Level_getTile(lvl, x, y, z);
    if (type == 0) {
        int changed = Level_setTile(lvl, x, y, z, lt->tileId);
        if (changed && depth < lt->spreadSpeed) {
            // recurse
            Liquid_tick(&lt->base, lvl, x, y, z);
            hasChanged = 1;
        }
    }
    return hasChanged;
}

/* neighbor reactions (mixing to rock) */

static void Liquid_neighborChanged(const Tile* self, Level* lvl, int x, int y, int z, int changedType) {
    const LiquidTile* lt = (const LiquidTile*)self;
    if (lt->liquidType == LIQ_WATER) {
        // touching lava -> rock
        if (changedType == 10 /*lava*/ || changedType == 11 /*calm lava*/) {
            Level_setTileNoUpdate(lvl, x, y, z, 1 /*rock*/);
            if (lvl->renderer) LevelRenderer_tileChanged(lvl->renderer, x, y, z);
        }
    } else { // lava
        if (changedType == 8 /*water*/ || changedType == 9 /*calm water*/) {
            Level_setTileNoUpdate(lvl, x, y, z, 1 /*rock*/);
            if (lvl->renderer) LevelRenderer_tileChanged(lvl->renderer, x, y, z);
        }
    }
}

/* Calm variant: on neighbor air, convert back to flowing; uses the same struct+render */

static void Calm_neighborChanged(const Tile* self, Level* lvl, int x, int y, int z, int changedType) {
    LiquidTile* lt = (LiquidTile*)self;

    // if any adjacent is air, revert to flowing
    if (Level_getTile(lvl, x-1,y,z)==0 || Level_getTile(lvl, x+1,y,z)==0 ||
        Level_getTile(lvl, x,y,z-1)==0 || Level_getTile(lvl, x,y,z+1)==0 ||
        Level_getTile(lvl, x,y-1,z)==0) {
        Level_setTileNoUpdate(lvl, x, y, z, lt->tileId);
        if (lvl->renderer) LevelRenderer_tileChanged(lvl->renderer, x, y, z);
    }

    // mixing to rock:
    if (lt->liquidType == LIQ_WATER) {
        if (changedType == 10 || changedType == 11) {
            Level_setTileNoUpdate(lvl, x, y, z, 1);
            if (lvl->renderer) LevelRenderer_tileChanged(lvl->renderer, x, y, z);
        }
    } else {
        if (changedType == 8 || changedType == 9) {
            Level_setTileNoUpdate(lvl, x, y, z, 1);
            if (lvl->renderer) LevelRenderer_tileChanged(lvl->renderer, x, y, z);
        }
    }
}

/* ---- base tile interface impls ------------------------------------------ */

static int Liquid_isSolid(const Tile* self)     { (void)self; return 0; }
static int Liquid_blocksLight(const Tile* self) { (void)self; return 1; }
static int Liquid_getAABB(const Tile* self, int x,int y,int z, AABB* out){ (void)self;(void)x;(void)y;(void)z;(void)out; return 0; }
static int Liquid_getLiquidType(const Tile* self) { return ((const LiquidTile*)self)->liquidType; }

/* ---- init --------------------------------------------------------------- */

void LiquidTile_init(LiquidTile* lt, int id, int liquidType, int isCalm) {
    // defaults shared with Tile_registerAll
    lt->base.id            = id;
    lt->base.textureId     = (liquidType == LIQ_LAVA) ? 30 : 14;
    lt->base.getTexture    = Liquid_getTexture;
    lt->base.render        = Liquid_render;
    lt->base.isSolid       = Liquid_isSolid;
    lt->base.blocksLight   = Liquid_blocksLight;
    lt->base.getAABB       = Liquid_getAABB;
    lt->base.mayPick       = Liquid_mayPick;
    lt->base.getLiquidType = Liquid_getLiquidType;

    lt->liquidType = liquidType;
    lt->tileId     = isCalm ? (id-1) : id;
    lt->calmTileId = isCalm ? id : (id+1);
    lt->spreadSpeed = (liquidType == LIQ_WATER) ? 8 : 2;

    Tile_setShape(&lt->base, 0.f, -0.1f, 0.f, 1.f, 0.9f, 1.f);

    if (isCalm) {
        lt->base.onTick          = NULL;
        lt->base.neighborChanged = Calm_neighborChanged;
    } else {
        lt->base.onTick          = Liquid_tick;
        lt->base.neighborChanged = Liquid_neighborChanged;
    }
}
