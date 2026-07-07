// particle/particle.h

#ifndef PARTICLE_H
#define PARTICLE_H

#include "../entity.h"
#include "../level/level.h"
#include "../renderer/tessellator.h"

typedef struct {
    Entity base;
    // its own xd yd zd, Entity_move never zeroes them
    float  xd, yd, zd;
    int    textureId;
    float  gravity;
    float  textureUOffset; float  textureVOffset;
    float  size;
    int    lifetime;
    int    age;
} Particle;

void Particle_init(Particle* p, Level* level,
                   float x, float y, float z,
                   float motionX, float motionY, float motionZ,
                   int textureId, float gravity);
void Particle_onTick(Particle* p);
void Particle_render(Particle* p, Tessellator* t, float partialTicks,
                     float cameraX, float cameraY, float cameraZ,
                     float cameraXWithY, float cameraZWithY);

#endif
