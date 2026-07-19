// particle/particle.h

#ifndef PARTICLE_H
#define PARTICLE_H

#include "../entity.h"
#include "../level/level.h"
#include "../level/tile/tile.h"
#include "../renderer/tessellator.h"

typedef struct {
    Entity base;
    int    textureId;
    float  textureUOffset; float  textureVOffset;
    float  size;
    int    lifetime;
    int    age;
    // c0.0.16a_02: per tile gravity multiplier (1.0 for most tiles, 0.4 for
    // Leaves), read from the source tile at construction instead of a flat
    // -0.04/tick fall for every particle
    float  gravity;

    // c0.27_st: WaterDropParticle and SmokeParticle (both new this version)
    // share this same engine/array instead of two whole parallel entity
    // types, since they're cosmetic only. Each has its own genuinely
    // distinct physics profile in real source, so Particle_onTick branches
    // on these flags instead of merging the behaviors
    bool isWaterDrop;
    bool isSmoke;

    // c0.27_st: real source's own ParticleEngine keeps two buckets, each
    // rendered in its own pass with its own texture bound: bucket 0 (this
    // flag true) samples particles.png (WaterDropParticle, SmokeParticle),
    // bucket 1 (false) samples terrain.png (TerrainParticle debris). The two
    // base classes also use different UV math: TerrainParticle offsets by
    // its own uo/vo into a quarter of the cell (a random fragment of the
    // source block's face), while the plain Particle.render() used by the
    // other two samples the whole cell with no uo/vo offset at all;
    // Particle_render branches on this same flag to pick the right formula
    bool usesParticlesAtlas;

    // c0.27_st: real source's own Particle class gained a self-tint (rCol/
    // gCol/bCol, 1.0 default) multiplied by the particle's own per-position
    // world brightness at render time, confirmed absent in c0.25_05_st's
    // equivalent class, genuinely new here. TerrainParticle-style debris
    // sets a fixed 0.6 dim tint; WaterDropParticle stays at the 1.0 default.
    // Was previously missing entirely (no color call at all), so particles
    // never darkened in caves the way every other lit thing in this port does
    float rCol, gCol, bCol;
} Particle;

void Particle_init(Particle* p, Level* level,
                   float x, float y, float z,
                   float motionX, float motionY, float motionZ,
                   const Tile* tile);

// c0.27_st: matches WaterDropParticle's own constructor exactly: runs the
// base Particle jitter/normalize ctor first (with zero input motion), then
// overrides xd/zd damping, a fresh yd pop, tex=16 (particles.png splash
// icon), the physics bounding box (0.01x0.01, NOT the separate render size
// field, which keeps the base ctor's own random 0.5-1.0 scale), and its own
// shorter lifetime formula
void Particle_initWaterDrop(Particle* p, Level* level, float x, float y, float z);

// c0.27_st: matches SmokeParticle's own constructor exactly: base Particle
// jitter ctor first (zero input motion), then its own 0.1x jitter damping, a
// random dark gray self-tint (0-0.3), and its own lifetime formula. tex
// starts at 0 (Java field default) and gets animated every tick after
void Particle_initSmoke(Particle* p, Level* level, float x, float y, float z);

void Particle_onTick(Particle* p);

void Particle_render(Particle* p, Tessellator* t, float partialTicks,
                     float cameraX, float cameraY, float cameraZ,
                     float cameraXWithY, float cameraZWithY);

#endif