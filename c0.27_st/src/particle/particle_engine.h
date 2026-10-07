#ifndef PARTICLE_ENGINE_H
#define PARTICLE_ENGINE_H

#include <GL/glew.h>

struct Level;   typedef struct Level Level;
struct Player;  typedef struct Player Player;

#include "particle.h"
#include "../renderer/tessellator.h"

// one list per atlas, 0 particles.png, 1 terrain.png
typedef struct ParticleEngine {
    Level* level;
    GLuint texture[2];
    Particle* items[2];
    int count[2], capacity[2];
    Tessellator tess;
} ParticleEngine;

void ParticleEngine_init  (ParticleEngine* pe, Level* level, GLuint terrainTex);
void ParticleEngine_onTick(ParticleEngine* pe);
void ParticleEngine_render(ParticleEngine* pe, const Player* player, float partialTicks);
void ParticleEngine_clear (ParticleEngine* pe);

void ParticleEngine_add(ParticleEngine* pe, const Particle* p);

#endif
