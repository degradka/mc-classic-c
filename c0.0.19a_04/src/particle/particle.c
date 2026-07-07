// particle/particle.c

#include "particle.h"
#include "../jrandom.h"
#include <math.h>

void Particle_init(Particle* p, Level* level,
                   float x, float y, float z,
                   float mx, float my, float mz, int textureId, float gravity)
{
    Entity_init(&p->base, level);
    p->textureId = textureId;
    p->gravity = gravity;

    Entity_setSize(&p->base, 0.2f, 0.2f);
    p->base.heightOffset = p->base.boundingBoxHeight / 2.0f;
    Entity_setPosition(&p->base, x, y, z);

    p->xd = mx + (float)(JRandom_mathRandom() * 2.0 - 1.0) * 0.4f;
    p->yd = my + (float)(JRandom_mathRandom() * 2.0 - 1.0) * 0.4f;
    p->zd = mz + (float)(JRandom_mathRandom() * 2.0 - 1.0) * 0.4f;

    float speed = (float)(JRandom_mathRandom() + JRandom_mathRandom() + 1.0) * 0.15f;
    float dd = (float)sqrt((double)(p->xd * p->xd + p->yd * p->yd + p->zd * p->zd));
    p->xd = p->xd / dd * speed * 0.4f;
    p->yd = p->yd / dd * speed * 0.4f + 0.1f;
    p->zd = p->zd / dd * speed * 0.4f;

    p->textureUOffset = (float)JRandom_mathRandom() * 3.0f;
    p->textureVOffset = (float)JRandom_mathRandom() * 3.0f;

    p->size     = (float)(JRandom_mathRandom() * 0.5 + 0.5);
    p->lifetime = (int)(4.0 / (JRandom_mathRandom() * 0.9 + 0.1));
    p->age      = 0;
}

void Particle_onTick(Particle* p) {
    Entity_onTick(&p->base);

    // a removed particle still moves this tick
    if (p->age++ >= p->lifetime) Entity_remove(&p->base);

    p->yd = (float)(p->yd - 0.04 * (double)p->gravity);
    Entity_move(&p->base, p->xd, p->yd, p->zd);

    p->xd *= 0.98f;
    p->yd *= 0.98f;
    p->zd *= 0.98f;

    if (p->base.onGround) {
        p->xd *= 0.7f;
        p->zd *= 0.7f;
    }
}

void Particle_render(Particle* p, Tessellator* tess, float partialTicks,
                     float xa, float ya, float za,
                     float xa2, float za2)
{
    float u0 = ((float)(p->textureId % 16) + p->textureUOffset / 4.0f) / 16.0f;
    float u1 = u0 + 0.015609375f;
    float v0 = ((float)(p->textureId / 16) + p->textureVOffset / 4.0f) / 16.0f;
    float v1 = v0 + 0.015609375f;

    float r = 0.1f * p->size;
    float x = p->base.prevX + (p->base.x - p->base.prevX) * partialTicks;
    float y = p->base.prevY + (p->base.y - p->base.prevY) * partialTicks;
    float z = p->base.prevZ + (p->base.z - p->base.prevZ) * partialTicks;

    Tessellator_vertexUV(tess, x - xa * r - xa2 * r, y - ya * r, z - za * r - za2 * r, u0, v1);
    Tessellator_vertexUV(tess, x - xa * r + xa2 * r, y + ya * r, z - za * r + za2 * r, u0, v0);
    Tessellator_vertexUV(tess, x + xa * r + xa2 * r, y + ya * r, z + za * r + za2 * r, u1, v0);
    Tessellator_vertexUV(tess, x + xa * r - xa2 * r, y - ya * r, z + za * r - za2 * r, u1, v1);
}
