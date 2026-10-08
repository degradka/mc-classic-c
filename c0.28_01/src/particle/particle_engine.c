// particle/particle_engine.c

#include "particle_engine.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../renderer/textures.h"
#include "../player.h"
#include "../math_helper.h"

void ParticleEngine_init(ParticleEngine* pe, Level* level, GLuint terrainTex) {
    pe->level = level;
    pe->texture[0] = loadTexture("resources/particles.png", GL_NEAREST);
    pe->texture[1] = terrainTex;
    for (int i = 0; i < 2; ++i) {
        pe->items[i] = NULL;
        pe->count[i] = pe->capacity[i] = 0;
    }
    Tessellator_begin(&pe->tess);
}

void ParticleEngine_clear(ParticleEngine* pe) {
    pe->count[0] = pe->count[1] = 0;
}

void ParticleEngine_add(ParticleEngine* pe, const Particle* p) {
    int t = Particle_texture(p);
    if (pe->count[t] == pe->capacity[t]) {
        int cap = pe->capacity[t] ? pe->capacity[t] * 2 : 64;
        Particle* n = (Particle*)realloc(pe->items[t], (size_t)cap * sizeof(Particle));
        if (!n) return;
        pe->items[t] = n;
        pe->capacity[t] = cap;
    }
    pe->items[t][pe->count[t]++] = *p;
}

void ParticleEngine_onTick(ParticleEngine* pe) {
    for (int t = 0; t < 2; ++t) {
        for (int i = 0; i < pe->count[t]; ) {
            Particle_onTick(&pe->items[t][i]);
            if (pe->items[t][i].base.removed) {
                // ordered remove so the draw order stays the same
                memmove(&pe->items[t][i], &pe->items[t][i + 1], (size_t)(pe->count[t] - i - 1) * sizeof(Particle));
                pe->count[t]--;
            } else {
                i++;
            }
        }
    }
}

void ParticleEngine_render(ParticleEngine* pe, const Player* player, float partialTicks) {
    float cameraX = -MathHelper_cos(player->e.yRotation * 3.1415927f / 180.0f);
    float cameraZ = -MathHelper_sin(player->e.yRotation * 3.1415927f / 180.0f);
    float cameraXWithY = -cameraZ * MathHelper_sin(player->e.xRotation * 3.1415927f / 180.0f);
    float cameraZWithY = cameraX * MathHelper_sin(player->e.xRotation * 3.1415927f / 180.0f);
    float cameraY = MathHelper_cos(player->e.xRotation * 3.1415927f / 180.0f);

    for (int t = 0; t < 2; ++t) {
        if (pe->count[t] == 0) continue;
        glBindTexture(GL_TEXTURE_2D, pe->texture[t]);
        Tessellator_begin(&pe->tess);
        for (int i = 0; i < pe->count[t]; ++i) {
            Particle_render(&pe->items[t][i], &pe->tess, partialTicks,
                            cameraX, cameraY, cameraZ, cameraXWithY, cameraZWithY);
        }
        Tessellator_end(&pe->tess);
    }
}
