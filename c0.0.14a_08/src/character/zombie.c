// character/zombie.c

#include "zombie.h"
#include "zombie_model.h"
#include "../renderer/textures.h"
#include "../timer.h"
#include <GL/glew.h>
#include <math.h>

static int texChar = 0;

// c0.0.14a_08 makes the model a shared static instance instead of a per
// Zombie member: every zombie alive in the same frame shares one pose.
static ZombieModel sModel;
static int sModelInit = 0;

static inline float frand01(void) { return (float)rand() / (float)RAND_MAX; }

void Zombie_init(Zombie* z, Level* level, float x, float y, float zpos) {
    Entity_init(&z->base, level);
    Entity_setPosition(&z->base, x, y, zpos);

    if (!sModelInit) { ZombieModel_init(&sModel); sModelInit = 1; }

    z->rotation = frand01() * (float)M_PI * 2.0f;
    z->rotationMotionFactor = (frand01() + 1.0f) * 0.01f;
    z->timeOffset = frand01() * 1239813.0f;
    z->speed = 1.0f;

    if (!texChar) texChar = loadTexture("resources/char.png", GL_NEAREST);
}

void Zombie_onTick(Zombie* z) {
    Entity_onTick(&z->base);

    if (z->base.y < -100.0f) Entity_remove(&z->base);

    z->rotation += z->rotationMotionFactor;
    z->rotationMotionFactor *= 0.99f;
    z->rotationMotionFactor += (frand01() - frand01()) * frand01() * frand01() * 0.07999999821186066f;

    const float v = sinf(z->rotation);
    const float f = cosf(z->rotation);

    if (z->base.onGround && frand01() < 0.08f) z->base.motionY = 0.5f;

    Entity_moveRelative(&z->base, v, f, z->base.onGround ? 0.1f : 0.02f);
    z->base.motionY -= 0.08f;

    Entity_move(&z->base, z->base.motionX, z->base.motionY, z->base.motionZ);

    z->base.motionX *= 0.91f;
    z->base.motionY *= 0.98f;
    z->base.motionZ *= 0.91f;

    if (z->base.onGround) {
        z->base.motionX *= 0.7f;
        z->base.motionZ *= 0.7f;
    }
}

void Zombie_render(const Zombie* z, float partialTicks) {
    glPushMatrix();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texChar);

    // c0.0.14a_08: tint by ambient brightness like every other entity now
    float b = Entity_getBrightness(&z->base);
    glColor3f(b, b, b);

    const double t  = (double)getCurrentTimeInNanoseconds() * 1e-9 * 10.0 * z->speed + z->timeOffset;
    const float ix = z->base.prevX + (z->base.x - z->base.prevX) * partialTicks;
    const float iy = z->base.prevY + (z->base.y - z->base.prevY) * partialTicks;
    const float iz = z->base.prevZ + (z->base.z - z->base.prevZ) * partialTicks;

    glTranslatef(ix, iy, iz);
    glScalef(1.f, -1.f, 1.f);

    const float size = 7.0f / 120.0f;
    glScalef(size, size, size);

    const double offY = fabs(sin(t * 0.6662)) * 5.0 + 23.0;
    glTranslated(0.0, -offY, 0.0);
    glRotated(z->rotation * 180.0 / M_PI + 180.0, 0, 1, 0);

    ZombieModel_render(&sModel, t);

    glDisable(GL_TEXTURE_2D);
    glPopMatrix();
}
