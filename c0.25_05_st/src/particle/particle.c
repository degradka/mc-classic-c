// particle/particle.c

#include "particle.h"
#include "../jrandom.h"
#include <math.h>
#include <stdlib.h>


void Particle_init(Particle* p, Level* level,
                   float x, float y, float z,
                   float mx, float my, float mz, const Tile* tile)
{
    Entity_init(&p->base, level);
    p->base.makeStepSound = false; // particles make no step sound
    p->textureId = tile->textureId;
    p->gravity = tile->particleGravity;

    // size & bbox
    p->base.boundingBoxWidth  = 0.2f;
    p->base.boundingBoxHeight = 0.2f;
    p->base.heightOffset = p->base.boundingBoxHeight * 0.5f;

    Entity_setPosition(&p->base, x, y, z);

    // initial motion + random jitter
    p->base.motionX = mx + (float)(JRandom_mathRandom() * 2.0 - 1.0) * 0.4f;
    p->base.motionY = my + (float)(JRandom_mathRandom() * 2.0 - 1.0) * 0.4f;
    p->base.motionZ = mz + (float)(JRandom_mathRandom() * 2.0 - 1.0) * 0.4f;

    double r1 = JRandom_mathRandom();
    double r2 = JRandom_mathRandom();
    float speed = (float)(r1 + r2 + 1.0) * 0.15f;
    float d = sqrtf(p->base.motionX*p->base.motionX +
                    p->base.motionY*p->base.motionY +
                    p->base.motionZ*p->base.motionZ);
    if (d > 0.0f) {
        p->base.motionX = p->base.motionX / d * speed * 0.4f;
        p->base.motionY = p->base.motionY / d * speed * 0.4f + 0.1f;
        p->base.motionZ = p->base.motionZ / d * speed * 0.4f;
    }

    p->textureUOffset = (float)JRandom_mathRandom() * 3.0f;
    p->textureVOffset = (float)JRandom_mathRandom() * 3.0f;

    p->size     = (float)(JRandom_mathRandom() * 0.5 + 0.5);
    p->lifetime = (int)(4.0 / (JRandom_mathRandom() * 0.9 + 0.1));
    p->age      = 0;
}

void Particle_setPower(Particle* p, float power) {
    p->base.motionX *= power;
    p->base.motionY = (p->base.motionY - 0.1f) * power + 0.1f;
    p->base.motionZ *= power;
}

// only shrinks the drawn size, the collision box stays as it was
void Particle_scale(Particle* p, float scale) {
    p->base.boundingBoxWidth = 0.2f * scale;
    p->base.boundingBoxHeight = 0.2f * scale;
    p->size *= scale;
}

void Particle_onTick(Particle* p) {
    Entity_onTick(&p->base);

    // a removed particle still moves on its last tick
    if (p->age++ >= p->lifetime) {
        Entity_remove(&p->base);
    }

    // gravity, scaled by the tile
    p->base.motionY = (float)((double)p->base.motionY - 0.04 * (double)p->gravity);

    // move
    float xd = p->base.motionX, yd = p->base.motionY, zd = p->base.motionZ;
    Entity_move(&p->base, xd, yd, zd);
    p->base.motionX = xd;
    p->base.motionY = yd;
    p->base.motionZ = zd;

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