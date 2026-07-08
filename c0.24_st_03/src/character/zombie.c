// character/zombie.c

#include "zombie.h"
#include "../renderer/textures.h"
#include "../timer.h"
#include "../jrandom.h"
#include <GL/glew.h>
#include <math.h>

static int texChar = 0;

void Zombie_init(Zombie* z, Level* level, float x, float y, float zpos) {
    Entity_init(&z->base, level);
    ZombieModel_init(&z->model);

    z->rotationMotionFactor = (float)(JRandom_mathRandom() + 1.0) * 0.01f;
    Entity_setPosition(&z->base, x, y, zpos);
    z->timeOffset = (float)JRandom_mathRandom() * 1239813.0f;
    z->rotation = (float)(JRandom_mathRandom() * M_PI * 2.0);
    z->speed = 1.0f;

    if (!texChar) texChar = loadTexture("resources/char.png", GL_NEAREST);
}

void Zombie_onTick(Zombie* z) {
    Entity_onTick(&z->base);

    if (z->base.y < -100.0f) Entity_remove(&z->base);

    z->rotation += z->rotationMotionFactor;
    z->rotationMotionFactor = (float)(z->rotationMotionFactor * 0.99);
    double r1 = JRandom_mathRandom(), r2 = JRandom_mathRandom();
    double r3 = JRandom_mathRandom(), r4 = JRandom_mathRandom();
    z->rotationMotionFactor = (float)(z->rotationMotionFactor + (r1 - r2) * r3 * r4 * 0.07999999821186066);

    float xa = (float)sin(z->rotation);
    float ya = (float)cos(z->rotation);

    if (z->base.onGround && JRandom_mathRandom() < 0.08) z->base.motionY = 0.5f;

    Entity_moveRelative(&z->base, xa, ya, z->base.onGround ? 0.1f : 0.02f);
    z->base.motionY = (float)(z->base.motionY - 0.08);
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
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texChar);
    glPushMatrix();

    double time = (double)getCurrentTimeInNanoseconds() / 1.0E9 * 10.0 * z->speed + z->timeOffset;
    float br = Entity_getBrightness(&z->base);
    glColor3f(br, br, br);
    float size = 0.058333334f;
    float yy = (float)(-fabs(sin(time * 0.6662)) * 5.0 - 23.0);

    glTranslatef(z->base.prevX + (z->base.x - z->base.prevX) * partialTicks,
                 z->base.prevY + (z->base.y - z->base.prevY) * partialTicks,
                 z->base.prevZ + (z->base.z - z->base.prevZ) * partialTicks);
    glScalef(1.0f, -1.0f, 1.0f);
    glTranslatef(0.0f, yy * size, 0.0f);
    glRotatef(z->rotation * 57.29578f + 180.0f, 0.0f, 1.0f, 0.0f);
    glScalef(-1.0f, 1.0f, 1.0f);

    ZombieModel_render(&z->model, (float)time, 1.0f, 0.0f, 0.0f, 0.0f, size);

    glPopMatrix();
    glDisable(GL_TEXTURE_2D);
}
