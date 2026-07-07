// particle/particle_engine.c

#include "particle_engine.h"
#include <stdlib.h>
#include <string.h>
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

void ParticleEngine_init(ParticleEngine* pe, Level* level, GLuint terrainTex) {
    pe->level = level;
    pe->texture = terrainTex;
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
            memmove(&pe->items[i], &pe->items[i + 1], (size_t)(pe->count - i - 1) * sizeof(Particle));
            pe->count--;
        } else {
            i++;
        }
    }
}

void ParticleEngine_render(ParticleEngine* pe, const Player* player, float partialTicks) {
    if (pe->count == 0) return;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, pe->texture);

    float cameraX = -(float)cos(player->e.yRotation * M_PI / 180.0);
    float cameraZ = -(float)sin(player->e.yRotation * M_PI / 180.0);
    float cameraXWithY = -cameraZ * (float)sin(player->e.xRotation * M_PI / 180.0);
    float cameraZWithY =  cameraX * (float)sin(player->e.xRotation * M_PI / 180.0);
    float cameraY = (float)cos(player->e.xRotation * M_PI / 180.0);

    Tessellator_begin(&pe->tess);
    for (int i = 0; i < pe->count; ++i) {
        Particle* p = &pe->items[i];
        float br = 0.8f * Entity_getBrightness(&p->base);
        Tessellator_color(&pe->tess, br, br, br);
        Particle_render(p, &pe->tess, partialTicks,
                        cameraX, cameraY, cameraZ,
                        cameraXWithY, cameraZWithY);
    }
    Tessellator_end(&pe->tess);

    glDisable(GL_TEXTURE_2D);
}