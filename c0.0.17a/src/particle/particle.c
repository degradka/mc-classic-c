// particle/particle.c

#include "particle.h"
#include <math.h>
#include <stdlib.h>

static inline float frand01(void) { return (float)rand() / (float)RAND_MAX; }

void Particle_init(Particle* p, Level* level,
                   float x, float y, float z,
                   float mx, float my, float mz, const Tile* tile)
{
    Entity_init(&p->base, level);
    p->textureId = tile->textureId;
    p->gravity = tile->particleGravity;

    // size & bbox
    p->base.boundingBoxWidth  = 0.2f;
    p->base.boundingBoxHeight = 0.2f;
    p->base.heightOffset = p->base.boundingBoxHeight * 0.5f;

    Entity_setPosition(&p->base, x, y, z);

    // initial motion + random jitter
    p->base.motionX = mx + (frand01() * 2.0f - 1.0f) * 0.4f;
    p->base.motionY = my + (frand01() * 2.0f - 1.0f) * 0.4f;
    p->base.motionZ = mz + (frand01() * 2.0f - 1.0f) * 0.4f;

    float speed = (frand01() + frand01() + 1.0f) * 0.15f;
    float d = sqrtf(p->base.motionX*p->base.motionX +
                    p->base.motionY*p->base.motionY +
                    p->base.motionZ*p->base.motionZ);
    // c0.0.14a_08 drops the zero guard here (divides unconditionally); kept
    // since a truly zero jittered vector is not reachable in practice and
    // this is strictly safer
    if (d > 0.0f) {
        p->base.motionX = p->base.motionX / d * speed * 0.4f;
        p->base.motionY = p->base.motionY / d * speed * 0.4f + 0.1f;
        p->base.motionZ = p->base.motionZ / d * speed * 0.4f;
    }

    p->textureUOffset = frand01() * 3.0f;
    p->textureVOffset = frand01() * 3.0f;

    p->size     = frand01() * 0.5f + 0.5f;
    p->lifetime = (int)(4.0 / (frand01() * 0.9 + 0.1));
    p->age      = 0;
}

void Particle_onTick(Particle* p) {
    Entity_onTick(&p->base);

    // c0.0.14a_08 drops the early return after removing on the final tick,
    // so a particle gets one extra physics update the tick it's removed on.
    // Harmless: it's dropped from the engine's list before ever rendering
    // again, ported to match anyway since it costs nothing.
    if (p->age++ >= p->lifetime) {
        Entity_remove(&p->base);
    }

    // gravity, scaled per source tile since c0.0.16a_02
    p->base.motionY -= 0.04f * p->gravity;

    // move
    Entity_move(&p->base, p->base.motionX, p->base.motionY, p->base.motionZ);

    // damping
    p->base.motionX *= 0.98f;
    p->base.motionY *= 0.98f;
    p->base.motionZ *= 0.98f;

    if (p->base.onGround) {
        p->base.motionX *= 0.7f;
        p->base.motionZ *= 0.7f;
    }
}

void Particle_render(Particle* p, Tessellator* tess, float partialTicks,
                     float cameraX, float cameraY, float cameraZ,
                     float cameraXWithY, float cameraZWithY)
{
    // UVs
    float minU = ( (p->textureId % 16) + p->textureUOffset * 0.25f ) / 16.0f;
    float maxU = minU + 999.0f / 64000.0f;
    float minV = ( (float)(p->textureId / 16) + p->textureVOffset * 0.25f ) / 16.0f;
    float maxV = minV + 999.0f / 64000.0f;

    // lerped position
    float x = (float)(p->base.prevX + (p->base.x - p->base.prevX) * partialTicks);
    float y = (float)(p->base.prevY + (p->base.y - p->base.prevY) * partialTicks);
    float z = (float)(p->base.prevZ + (p->base.z - p->base.prevZ) * partialTicks);

    float s = p->size * 0.1f;

    Tessellator_vertexUV(tess, x - cameraX * s - cameraXWithY * s, y - cameraY * s, z - cameraZ * s - cameraZWithY * s, minU, maxV);
    Tessellator_vertexUV(tess, x - cameraX * s + cameraXWithY * s, y + cameraY * s, z - cameraZ * s + cameraZWithY * s, minU, minV);
    Tessellator_vertexUV(tess, x + cameraX * s + cameraXWithY * s, y + cameraY * s, z + cameraZ * s + cameraZWithY * s, maxU, minV);
    Tessellator_vertexUV(tess, x + cameraX * s - cameraXWithY * s, y - cameraY * s, z + cameraZ * s - cameraZWithY * s, maxU, maxV);
}