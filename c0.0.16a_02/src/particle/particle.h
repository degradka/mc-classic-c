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
} Particle;

void Particle_init(Particle* p, Level* level,
                   float x, float y, float z,
                   float motionX, float motionY, float motionZ,
                   const Tile* tile);

void Particle_onTick(Particle* p);

void Particle_render(Particle* p, Tessellator* t, float partialTicks,
                     float cameraX, float cameraY, float cameraZ,
                     float cameraXWithY, float cameraZWithY);

#endif