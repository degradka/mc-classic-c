// item/primed_tnt.c

#include "primed_tnt.h"
#include "../jrandom.h"
#include "../level/tile/tile.h"
#include "../renderer/textures.h"
#include "../renderer/tessellator.h"
#include <GL/glew.h>
#include <math.h>

extern void Minecraft_spawnTntParticle(Level* lvl, float x, float y, float z, float mx, float my, float mz);
extern void Minecraft_spawnSmoke(Level* lvl, float x, float y, float z);
extern int Minecraft_getTerrainTexture(void);

static Tessellator sTess;

void PrimedTnt_init(PrimedTnt* t, Level* level, float x, float y, float z) {
    Entity* e = &t->e;
    Entity_init(e, level);
    Entity_setSize(e, 0.98f, 0.98f);
    e->heightOffset = e->boundingBoxHeight / 2.0f;
    Entity_setPosition(e, x, y, z);
    // radians fed to a degrees formula
    float a = (float)(JRandom_mathRandom() * M_PI * 2.0);
    t->xd = -((float)sin((double)a * M_PI / 180.0)) * 0.02f;
    t->yd = 0.2f;
    t->zd = -((float)cos((double)a * M_PI / 180.0)) * 0.02f;
    e->makeStepSound = false;
    t->life = 40;
    e->prevX = x;
    e->prevY = y;
    e->prevZ = z;
}

void PrimedTnt_onTick(PrimedTnt* t) {
    Entity* e = &t->e;
    e->prevX = e->x;
    e->prevY = e->y;
    e->prevZ = e->z;
    t->yd -= 0.04f;
    Entity_move(e, t->xd, t->yd, t->zd);
    t->xd *= 0.98f;
    t->yd *= 0.98f;
    t->zd *= 0.98f;
    if (e->onGround) {
        t->xd *= 0.7f;
        t->zd *= 0.7f;
        t->yd *= -0.5f;
    }
    if (t->life-- <= 0) {
        Entity_remove(e);
        JRandom random;
        JRandom_initRandom(&random);
        const float radius = 4.0f;
        Level_explode(e->level, NULL, e->x, e->y, e->z, radius);
        for (int i = 0; i < 100; ++i) {
            float dx = (float)JRandom_nextGaussian(&random) * radius / 4.0f;
            float dy = (float)JRandom_nextGaussian(&random) * radius / 4.0f;
            float dz = (float)JRandom_nextGaussian(&random) * radius / 4.0f;
            float d = (float)sqrt(dx * dx + dy * dy + dz * dz);
            Minecraft_spawnTntParticle(e->level, e->x + dx, e->y + dy, e->z + dz, dx / d / d, dy / d / d, dz / d / d);
        }
        return;
    }
    Minecraft_spawnSmoke(e->level, e->x, e->y + 0.6f, e->z);
}

void PrimedTnt_render(const PrimedTnt* t, float partialTicks) {
    const Entity* e = &t->e;
    glBindTexture(GL_TEXTURE_2D, Minecraft_getTerrainTexture());
    float br = Level_getBrightness(e->level, (int)e->x, (int)e->y, (int)e->z);
    glPushMatrix();
    glColor4f(br, br, br, 1.0f);
    glTranslatef(e->prevX + (e->x - e->prevX) * partialTicks - 0.5f,
                 e->prevY + (e->y - e->prevY) * partialTicks - 0.5f,
                 e->prevZ + (e->z - e->prevZ) * partialTicks - 0.5f);
    glPushMatrix();
    TILE_TNT.renderItem(&TILE_TNT, &sTess);
    // the white fuse flash drawn over it
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glColor4f(1.0f, 1.0f, 1.0f, (float)((t->life / 4 + 1) % 2) * 0.4f);
    if (t->life <= 16) glColor4f(1.0f, 1.0f, 1.0f, (float)((t->life + 1) % 2) * 0.6f);
    if (t->life <= 2) glColor4f(1.0f, 1.0f, 1.0f, 0.9f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    TILE_TNT.renderItem(&TILE_TNT, &sTess);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
    glPopMatrix();
    glPopMatrix();
}
