// particle/particle_engine.h

#ifndef PARTICLE_ENGINE_H
#define PARTICLE_ENGINE_H

#include <GL/glew.h>

struct Level;   typedef struct Level Level;
struct Player;  typedef struct Player Player;

#include "particle.h"
#include "../renderer/tessellator.h"

typedef struct ParticleEngine {
    Level* level;
    GLuint texture;         // terrain.png, TerrainParticle debris
    GLuint particlesTexture; // particles.png, WaterDropParticle, SmokeParticle
    Particle* items;
    int count, capacity;
    Tessellator tess;
} ParticleEngine;

void ParticleEngine_init  (ParticleEngine* pe, Level* level, GLuint terrainTex, GLuint particlesTex);
void ParticleEngine_onTick(ParticleEngine* pe);
// c0.0.14a_08 drops the lit/shadow layer split (replaced by per particle
// brightness tinting), so this is now a single unconditional pass
void ParticleEngine_render(ParticleEngine* pe, const Player* player, float partialTicks);

void ParticleEngine_add(ParticleEngine* pe, const Particle* p);

#endif