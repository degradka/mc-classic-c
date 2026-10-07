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
    // particle gravity from the tile, 0.4 for leaves
    float  gravity;
} Particle;

void Particle_init(Particle* p, Level* level,
                   float x, float y, float z,
                   float motionX, float motionY, float motionZ,
                   const Tile* tile);

void Particle_setPower(Particle* p, float power);
void Particle_scale(Particle* p, float scale);
void Particle_onTick(Particle* p);

void Particle_render(Particle* p, Tessellator* t, float partialTicks,
                     float cameraX, float cameraY, float cameraZ,
                     float cameraXWithY, float cameraZWithY);

#endif