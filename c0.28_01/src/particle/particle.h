// particle/particle.h: Particle and its TerrainParticle, SmokeParticle and WaterDropParticle subclasses

#ifndef PARTICLE_H
#define PARTICLE_H

#include "../entity.h"
#include "../level/level.h"
#include "../level/tile/tile.h"
#include "../renderer/tessellator.h"

typedef enum { PARTICLE_TERRAIN, PARTICLE_WATER_DROP } ParticleKind;

typedef struct {
    Entity base;
    ParticleKind kind;
    int    textureId;
    float  textureUOffset; float  textureVOffset;
    float  size;
    int    lifetime;
    int    age;
    float  gravity;
    float  r, g, b;
} Particle;

// a block break or punch bit, terrain.png
void Particle_initTerrain(Particle* p, Level* level, float x, float y, float z,
                          float motionX, float motionY, float motionZ, const Tile* tile);
// particles.png
void Particle_initWaterDrop(Particle* p, Level* level, float x, float y, float z);

// 0 particles.png, 1 terrain.png
int  Particle_texture(const Particle* p);
void Particle_setPower(Particle* p, float power);
void Particle_scale(Particle* p, float scale);
void Particle_onTick(Particle* p);

void Particle_render(Particle* p, Tessellator* t, float partialTicks,
                     float cameraX, float cameraY, float cameraZ,
                     float cameraXWithY, float cameraZWithY);

#endif
