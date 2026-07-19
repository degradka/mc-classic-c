// particle/particle.c

#include "particle.h"
#include <math.h>
#include <stdlib.h>

static inline float frand01(void) { return (float)rand() / (float)RAND_MAX; }

// matches the base Particle(Level,x,y,z,mx,my,mz) constructor exactly: the
// jittered/normalized initial motion, the random uo/vo sub-cell offset (only
// ever consumed by TerrainParticle's own render override), the random 0.5-1.0
// render scale, and the default 4.0/(random*0.9+0.1) lifetime. WaterDropParticle
// and SmokeParticle both call this with mx=my=mz=0 and then override fields
// on top, exactly like their own real constructors chaining to super()
static void Particle_initBase(Particle* p, Level* level,
                              float x, float y, float z,
                              float mx, float my, float mz)
{
    Entity_init(&p->base, level);
    p->base.makeStepSound = false; // c0.0.23a_01: particles never play footstep sounds
    p->base.boundingBoxWidth  = 0.2f;
    p->base.boundingBoxHeight = 0.2f;
    p->base.heightOffset = p->base.boundingBoxHeight * 0.5f;
    Entity_setPosition(&p->base, x, y, z);

    p->rCol = p->gCol = p->bCol = 1.0f;

    p->base.motionX = mx + (frand01() * 2.0f - 1.0f) * 0.4f;
    p->base.motionY = my + (frand01() * 2.0f - 1.0f) * 0.4f;
    p->base.motionZ = mz + (frand01() * 2.0f - 1.0f) * 0.4f;

    float speed = (frand01() + frand01() + 1.0f) * 0.15f;
    float d = sqrtf(p->base.motionX*p->base.motionX +
                    p->base.motionY*p->base.motionY +
                    p->base.motionZ*p->base.motionZ);
    p->base.motionX = p->base.motionX / d * speed * 0.4f;
    p->base.motionY = p->base.motionY / d * speed * 0.4f + 0.1f;
    p->base.motionZ = p->base.motionZ / d * speed * 0.4f;

    p->textureUOffset = frand01() * 3.0f;
    p->textureVOffset = frand01() * 3.0f;

    p->size     = frand01() * 0.5f + 0.5f;
    p->lifetime = (int)(4.0 / (frand01() * 0.9 + 0.1));
    p->age      = 0;
    p->textureId = 0;
    p->gravity  = 0.0f; // matches the base Particle class's own unset (Java default 0) field

    p->isWaterDrop = false;
    p->isSmoke = false;
    p->usesParticlesAtlas = false;
}

void Particle_init(Particle* p, Level* level,
                   float x, float y, float z,
                   float mx, float my, float mz, const Tile* tile)
{
    Particle_initBase(p, level, x, y, z, mx, my, mz);
    // matches TerrainParticle's own constructor tail: tex/gravity read from
    // the source tile, and a fixed dim self-tint (0.6) that real source
    // gives every piece of block debris
    p->textureId = tile->textureId;
    p->gravity = tile->particleGravity;
    p->rCol = p->gCol = p->bCol = 0.6f;
}

void Particle_initWaterDrop(Particle* p, Level* level, float x, float y, float z) {
    Particle_initBase(p, level, x, y, z, 0.0f, 0.0f, 0.0f);
    p->base.motionX *= 0.3f;
    p->base.motionY = frand01() * 0.2f + 0.1f;
    p->base.motionZ *= 0.3f;
    p->rCol = p->gCol = p->bCol = 1.0f; // matches WaterDropParticle's own explicit (redundant) override
    p->textureId = 16; // particles.png splash icon
    // CORRECTION: this is the physics collision box only. The separate
    // render-scale `size` field is NOT overridden by WaterDropParticle's own
    // real constructor; it inherits the base ctor's random 0.5-1.0 scale
    // like any other particle. This was wrongly set to 0.01 too before,
    // rendering at roughly 1/70th the intended size, reading as "rain splash
    // doesn't exist"
    p->base.boundingBoxWidth  = 0.01f;
    p->base.boundingBoxHeight = 0.01f;
    p->base.heightOffset = p->base.boundingBoxHeight * 0.5f;
    p->lifetime = (int)(8.0 / (frand01() * 0.8 + 0.2));
    p->isWaterDrop = true;
    p->usesParticlesAtlas = true;
}

void Particle_initSmoke(Particle* p, Level* level, float x, float y, float z) {
    Particle_initBase(p, level, x, y, z, 0.0f, 0.0f, 0.0f);
    p->base.motionX *= 0.1f;
    p->base.motionY *= 0.1f;
    p->base.motionZ *= 0.1f;
    p->rCol = p->gCol = p->bCol = frand01() * 0.3f;
    p->lifetime = (int)(8.0 / (frand01() * 0.8 + 0.2));
    p->isSmoke = true;
    p->usesParticlesAtlas = true;
}

void Particle_onTick(Particle* p) {
    Entity_onTick(&p->base);

    if (p->isWaterDrop) {
        // matches WaterDropParticle's own tick() exactly: flat fall rate
        // (not gravity scaled), and a 50% chance to vanish the instant it
        // lands rather than always lingering the rest of its lifetime
        p->base.motionY -= 0.06f;
        Entity_move(&p->base, p->base.motionX, p->base.motionY, p->base.motionZ);
        p->base.motionX *= 0.98f;
        p->base.motionY *= 0.98f;
        p->base.motionZ *= 0.98f;
        if (p->age++ >= p->lifetime) {
            Entity_remove(&p->base);
        }
        if (p->base.onGround) {
            if (frand01() < 0.5f) {
                Entity_remove(&p->base);
            }
            p->base.motionX *= 0.7f;
            p->base.motionZ *= 0.7f;
        }
        return;
    }

    if (p->isSmoke) {
        // matches SmokeParticle's own tick() exactly: age is incremented (and
        // checked against lifetime) BEFORE the animated texture index is
        // computed from it, an upward-accelerating drift (opposite of every
        // other particle's downward gravity), and a slower 0.96 damping with
        // no gravity term at all
        if (p->age++ >= p->lifetime) {
            Entity_remove(&p->base);
        }
        p->textureId = 7 - (p->age << 3) / p->lifetime;
        p->base.motionY += 0.004f;
        Entity_move(&p->base, p->base.motionX, p->base.motionY, p->base.motionZ);
        p->base.motionX *= 0.96f;
        p->base.motionY *= 0.96f;
        p->base.motionZ *= 0.96f;
        if (p->base.onGround) {
            p->base.motionX *= 0.7f;
            p->base.motionZ *= 0.7f;
        }
        return;
    }

    // c0.0.14a_08 drops the early return after removing on the final tick,
    // so a particle gets one extra physics update the tick it's removed on.
    // Harmless: it's dropped from the engine's list before ever rendering
    // again, ported to match anyway since it costs nothing.
    if (p->age++ >= p->lifetime) {
        Entity_remove(&p->base);
    }

    // gravity, scaled per source tile since c0.0.16a_02
    p->base.motionY -= 0.04f * p->gravity;

    // move
    Entity_move(&p->base, p->base.motionX, p->base.motionY, p->base.motionZ);

    // damping
    p->base.motionX *= 0.98f;
    p->base.motionY *= 0.98f;
    p->base.motionZ *= 0.98f;

    if (p->base.onGround) {
        p->base.motionX *= 0.7f;
        p->base.motionZ *= 0.7f;
    }
}

void Particle_render(Particle* p, Tessellator* tess, float partialTicks,
                     float cameraX, float cameraY, float cameraZ,
                     float cameraXWithY, float cameraZWithY)
{
    // UVs: TerrainParticle debris offsets by its own uo/vo into a quarter of
    // the cell (a random fragment of the source block's face); the base
    // Particle.render() used by WaterDropParticle/SmokeParticle samples the
    // whole cell with no uo/vo offset at all; two genuinely different
    // formulas in real source, not one shared one
    float minU, maxU, minV, maxV;
    if (p->usesParticlesAtlas) {
        minU = (float)(p->textureId % 16) / 16.0f;
        maxU = minU + 999.0f / 16000.0f;
        minV = (float)(p->textureId / 16) / 16.0f;
        maxV = minV + 999.0f / 16000.0f;
    } else {
        minU = ( (p->textureId % 16) + p->textureUOffset * 0.25f ) / 16.0f;
        maxU = minU + 999.0f / 64000.0f;
        minV = ( (float)(p->textureId / 16) + p->textureVOffset * 0.25f ) / 16.0f;
        maxV = minV + 999.0f / 64000.0f;
    }

    // lerped position
    float x = (float)(p->base.prevX + (p->base.x - p->base.prevX) * partialTicks);
    float y = (float)(p->base.prevY + (p->base.y - p->base.prevY) * partialTicks);
    float z = (float)(p->base.prevZ + (p->base.z - p->base.prevZ) * partialTicks);

    float s = p->size * 0.1f;

    // c0.27_st: matches Particle's own render() exactly: self-tint
    // multiplied by this particle's own per-position world brightness, so
    // it darkens in caves the same as every other lit thing in this port.
    // Previously missing entirely (no color call at all)
    float brightness = Entity_getBrightness(&p->base);
    Tessellator_color(tess, p->rCol * brightness, p->gCol * brightness, p->bCol * brightness);

    Tessellator_vertexUV(tess, x - cameraX * s - cameraXWithY * s, y - cameraY * s, z - cameraZ * s - cameraZWithY * s, minU, maxV);
    Tessellator_vertexUV(tess, x - cameraX * s + cameraXWithY * s, y + cameraY * s, z - cameraZ * s + cameraZWithY * s, minU, minV);
    Tessellator_vertexUV(tess, x + cameraX * s + cameraXWithY * s, y + cameraY * s, z + cameraZ * s + cameraZWithY * s, maxU, minV);
    Tessellator_vertexUV(tess, x + cameraX * s - cameraXWithY * s, y - cameraY * s, z + cameraZ * s - cameraZWithY * s, maxU, maxV);
}
