// particle/particle_engine.c

#include "particle_engine.h"
#include <stdlib.h>
#include <math.h>
#include "../renderer/textures.h"
#include "../player.h"

static void ensure_capacity(ParticleEngine* pe, int need) {
    if (pe->capacity >= need) return;
    int newcap = pe->capacity ? pe->capacity * 2 : 64;
    if (newcap < need) newcap = need;
    pe->items = (Particle*)realloc(pe->items, (size_t)newcap * sizeof(Particle));
    pe->capacity = newcap;
}

void ParticleEngine_init(ParticleEngine* pe, Level* level, GLuint terrainTex, GLuint particlesTex) {
    pe->level = level;
    pe->texture = terrainTex;
    pe->particlesTexture = particlesTex;
    pe->items = NULL;
    pe->count = pe->capacity = 0;
    Tessellator_begin(&pe->tess);
}

void ParticleEngine_add(ParticleEngine* pe, const Particle* p) {
    ensure_capacity(pe, pe->count + 1);
    pe->items[pe->count++] = *p;
}

void ParticleEngine_onTick(ParticleEngine* pe) {
    for (int i = 0; i < pe->count; ) {
        Particle_onTick(&pe->items[i]);
        if (pe->items[i].base.removed) {
            pe->items[i] = pe->items[pe->count - 1];
            pe->count--;
        } else {
            i++;
        }
    }
}

void ParticleEngine_render(ParticleEngine* pe, const Player* player, float partialTicks) {
    if (pe->count == 0) return;

    glEnable(GL_TEXTURE_2D);
    glDisableClientState(GL_COLOR_ARRAY);
    GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    if (cullWasEnabled) glDisable(GL_CULL_FACE);

    // camera facing vectors so each particle billboards toward the player
    float yawRad   = player->e.yRotation * (float)M_PI / 180.0f;
    float pitchRad = player->e.xRotation * (float)M_PI / 180.0f;

    float cameraX = -cosf(yawRad);
    float cameraY =  cosf(pitchRad);
    float cameraZ = -sinf(yawRad);

    float cameraXWithY = -cameraZ * sinf(pitchRad);
    float cameraZWithY =  cameraX * sinf(pitchRad);

    // c0.27_st: real source's own ParticleEngine keeps two buckets: bucket 0
    // (WaterDropParticle/SmokeParticle, particles.png) and bucket 1
    // (TerrainParticle debris, terrain.png), and renders each in its own
    // begin/bind/end pass so both atlases can coexist in the same frame.
    // This port keeps one flat array instead of two lists (a particle's
    // bucket membership never changes over its life, so filtering per pass
    // is equivalent), matching bucket order 0 then 1
    for (int bucket = 0; bucket < 2; ++bucket) {
        bool wantParticlesAtlas = (bucket == 0);
        bool any = false;
        for (int i = 0; i < pe->count; ++i) {
            if (pe->items[i].usesParticlesAtlas == wantParticlesAtlas) { any = true; break; }
        }
        if (!any) continue;

        glBindTexture(GL_TEXTURE_2D, wantParticlesAtlas ? pe->particlesTexture : pe->texture);

        Tessellator_begin(&pe->tess);
        for (int i = 0; i < pe->count; ++i) {
            Particle* p = &pe->items[i];
            if (p->usesParticlesAtlas != wantParticlesAtlas) continue;
            // c0.27_st: Particle_render now sets its own per-particle self-tint
            // (rCol/gCol/bCol) times its own per-position brightness, matching real
            // source's own Particle.render() exactly. That superseded this
            // call's own flat 0.8-for-everyone tint (matching c0.0.14a_08
            // through c0.25_05_st, before per-particle tints existed)
            Particle_render(p, &pe->tess, partialTicks,
                            cameraX, cameraY, cameraZ,
                            cameraXWithY, cameraZWithY);
        }
        Tessellator_end(&pe->tess);
    }

    if (cullWasEnabled) glEnable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
}