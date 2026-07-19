// item/primed_tnt.c

#include "primed_tnt.h"
#include "../level/level.h"
#include "../level/tile/tile.h"
#include "../renderer/tessellator.h"
#include "../renderer/textures.h"
#include <GL/glew.h>
#include <math.h>
#include <stdlib.h>

static Tessellator sTess;

static inline float frand01(void) { return (float)rand() / (float)RAND_MAX; }

// Random.nextGaussian, Box-Muller
static float gaussianRand(void) {
    float u1 = frand01();
    if (u1 < 1e-7f) u1 = 1e-7f;
    float u2 = frand01();
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
}

extern void Minecraft_spawnExplosionParticle(Level* lvl, float x, float y, float z, float mx, float my, float mz, const Tile* tile);
// implemented in minecraft.c: spawns a SmokeParticle into particleEngine
extern void Minecraft_spawnSmokeParticle(Level* lvl, float x, float y, float z);

void PrimedTnt_init(PrimedTnt* t, Level* level, float x, float y, float z) {
    Entity* e = &t->e;
    Entity_init(e, level);
    Entity_setSize(e, 0.98f, 0.98f);
    e->heightOffset = 0.49f; // bbHeight/2
    Entity_setPosition(e, x, y, z);

    float angle = (float)((double)rand() / RAND_MAX * M_PI * 2.0);
    e->motionX = -sinf(angle * (float)M_PI / 180.0f) * 0.02f;
    e->motionY = 0.2f;
    e->motionZ = -cosf(angle * (float)M_PI / 180.0f) * 0.02f;

    e->makeStepSound = false;
    t->fuse = 40; // 40 tick fuse
}

extern void Level_explode(Level* level, Entity* source, float x, float y, float z, float radius);

void PrimedTnt_onTick(PrimedTnt* t) {
    Entity* e = &t->e;
    e->prevX = e->x; e->prevY = e->y; e->prevZ = e->z;

    e->motionY -= 0.04f;
    Entity_move(e, e->motionX, e->motionY, e->motionZ);
    e->motionX *= 0.98f;
    e->motionY *= 0.98f;
    e->motionZ *= 0.98f;
    if (e->onGround) {
        e->motionX *= 0.7f;
        e->motionZ *= 0.7f;
        e->motionY *= -0.5f;
    }

    // life-- <= 0, so it goes off one tick after reaching 0
    if (t->fuse-- <= 0) {
        Entity_remove(e);
        const float radius = 4.0f;
        Level_explode(e->level, NULL, e->x, e->y, e->z, radius);

        for (int i = 0; i < 100; ++i) {
            float dx = gaussianRand() * radius / 4.0f;
            float dy = gaussianRand() * radius / 4.0f;
            float dz = gaussianRand() * radius / 4.0f;
            float mag = sqrtf(dx * dx + dy * dy + dz * dz);
            if (mag < 0.0001f) mag = 0.0001f;
            float mx = dx / mag / mag;
            float my = dy / mag / mag;
            float mz = dz / mag / mag;
            Minecraft_spawnExplosionParticle(e->level, e->x + dx, e->y + dy, e->z + dz, mx, my, mz, &TILE_TNT);
        }
    } else {
        // a smoke particle above it every tick it doesn't explode
        Minecraft_spawnSmokeParticle(e->level, e->x, e->y + 0.6f, e->z);
    }
}

void PrimedTnt_render(const PrimedTnt* t, float partialTicks) {
    const Entity* e = &t->e;
    float brightness = Level_getBrightness(e->level, (int)e->x, (int)e->y, (int)e->z);
    int life = t->fuse;

    float ix = e->prevX + (e->x - e->prevX) * partialTicks;
    float iy = e->prevY + (e->y - e->prevY) * partialTicks;
    float iz = e->prevZ + (e->z - e->prevZ) * partialTicks;

    static int tex = 0;
    if (!tex) tex = loadTexture("resources/terrain.png", GL_NEAREST);

    glPushMatrix();
    glColor4f(brightness, brightness, brightness, 1.0f);
    glTranslatef(ix - 0.5f, iy - 0.5f, iz - 0.5f);

    glPushMatrix();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glDisable(GL_LIGHTING);
    // drawn normally first
    TILE_TNT.renderItem(&TILE_TNT, &sTess, brightness);

    // the white fuse flash drawn over it
    glDisable(GL_TEXTURE_2D);

    // the white flash: every 4 ticks at 0.4, every tick at 0.6 under 16, steady 0.9 under 2
    float alpha = (float)((life / 4 + 1) % 2) * 0.4f;
    if (life <= 16) alpha = (float)((life + 1) % 2) * 0.6f;
    if (life <= 2)  alpha = 0.9f;

    glColor4f(1.0f, 1.0f, 1.0f, alpha);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    // ignoreColor so the alpha from glColor4f gets through
    Tessellator_setIgnoreColor(&sTess, 1);
    TILE_TNT.renderItem(&TILE_TNT, &sTess, 1.0f);
    Tessellator_setIgnoreColor(&sTess, 0);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);

    glEnable(GL_TEXTURE_2D);
    glPopMatrix();
    glPopMatrix();
}
