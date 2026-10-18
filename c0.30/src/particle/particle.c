// particle/particle.c

#include "particle.h"
#include "../jrandom.h"
#include "../math_helper.h"
#include <math.h>
#include <stdlib.h>

// the Particle constructor every kind runs first
static void initBase(Particle* p, Level* level, float x, float y, float z, float mx, float my, float mz) {
    Entity_init(&p->base, level);
    p->base.boundingBoxWidth  = 0.2f;
    p->base.boundingBoxHeight = 0.2f;
    p->base.heightOffset = p->base.boundingBoxHeight / 2.0f;
    Entity_setPosition(&p->base, x, y, z);
    p->r = p->g = p->b = 1.0f;
    p->textureId = 0;
    p->gravity = 0.0f;

    p->base.motionX = mx + (float)(JRandom_mathRandom() * 2.0 - 1.0) * 0.4f;
    p->base.motionY = my + (float)(JRandom_mathRandom() * 2.0 - 1.0) * 0.4f;
    p->base.motionZ = mz + (float)(JRandom_mathRandom() * 2.0 - 1.0) * 0.4f;

    double r1 = JRandom_mathRandom();
    double r2 = JRandom_mathRandom();
    float speed = (float)(r1 + r2 + 1.0) * 0.15f;
    float d = MathHelper_sqrt(p->base.motionX * p->base.motionX + p->base.motionY * p->base.motionY +
                              p->base.motionZ * p->base.motionZ);
    p->base.motionX = p->base.motionX / d * speed * 0.4f;
    p->base.motionY = p->base.motionY / d * speed * 0.4f + 0.1f;
    p->base.motionZ = p->base.motionZ / d * speed * 0.4f;

    p->textureUOffset = (float)JRandom_mathRandom() * 3.0f;
    p->textureVOffset = (float)JRandom_mathRandom() * 3.0f;
    p->size     = (float)(JRandom_mathRandom() * 0.5 + 0.5);
    p->lifetime = (int)(4.0 / (JRandom_mathRandom() * 0.9 + 0.1));
    p->age      = 0;
    p->base.makeStepSound = false;
}

void Particle_initTerrain(Particle* p, Level* level, float x, float y, float z,
                          float mx, float my, float mz, const Tile* tile) {
    initBase(p, level, x, y, z, mx, my, mz);
    p->kind = PARTICLE_TERRAIN;
    p->textureId = tile->textureId;
    p->gravity = tile->particleGravity;
    p->r = p->g = p->b = 0.6f;
}

void Particle_initSmoke(Particle* p, Level* level, float x, float y, float z) {
    initBase(p, level, x, y, z, 0.0f, 0.0f, 0.0f);
    p->kind = PARTICLE_SMOKE;
    p->base.motionX *= 0.1f;
    p->base.motionY *= 0.1f;
    p->base.motionZ *= 0.1f;
    p->g = p->b = (float)(JRandom_mathRandom() * (double)0.3f);
    p->r = p->b;
    p->lifetime = (int)(8.0 / (JRandom_mathRandom() * 0.8 + 0.2));
    p->base.noPhysics = true;
}

void Particle_initWaterDrop(Particle* p, Level* level, float x, float y, float z) {
    initBase(p, level, x, y, z, 0.0f, 0.0f, 0.0f);
    p->kind = PARTICLE_WATER_DROP;
    p->base.motionX *= 0.3f;
    p->base.motionY = (float)JRandom_mathRandom() * 0.2f + 0.1f;
    p->base.motionZ *= 0.3f;
    p->r = p->g = p->b = 1.0f;
    p->textureId = 16;
    Entity_setSize(&p->base, 0.01f, 0.01f);
    p->lifetime = (int)(8.0 / (JRandom_mathRandom() * 0.8 + 0.2));
}

int Particle_texture(const Particle* p) {
    return p->kind == PARTICLE_TERRAIN ? 1 : 0;
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

static void move(Particle* p) {
    float xd = p->base.motionX, yd = p->base.motionY, zd = p->base.motionZ;
    Entity_move(&p->base, xd, yd, zd);
    p->base.motionX = xd;
    p->base.motionY = yd;
    p->base.motionZ = zd;
}

void Particle_onTick(Particle* p) {
    Entity_onTick(&p->base);
    Entity* e = &p->base;

    if (p->kind == PARTICLE_WATER_DROP) {
        e->motionY = (float)((double)e->motionY - 0.06);
        move(p);
        e->motionX *= 0.98f;
        e->motionY *= 0.98f;
        e->motionZ *= 0.98f;
        if (p->lifetime-- <= 0) Entity_remove(e);
        if (e->onGround) {
            if (JRandom_mathRandom() < 0.5) Entity_remove(e);
            e->motionX *= 0.7f;
            e->motionZ *= 0.7f;
        }
        return;
    }

    // the removed particle still moves this tick
    if (p->age++ >= p->lifetime) Entity_remove(e);

    float drag = 0.98f;
    if (p->kind == PARTICLE_SMOKE) {
        p->textureId = 7 - (p->age << 3) / p->lifetime;
        e->motionY = (float)((double)e->motionY + 0.004);
        drag = 0.96f;
    } else {
        e->motionY = (float)((double)e->motionY - 0.04 * (double)p->gravity);
    }
    move(p);
    e->motionX *= drag;
    e->motionY *= drag;
    e->motionZ *= drag;
    if (e->onGround) {
        e->motionX *= 0.7f;
        e->motionZ *= 0.7f;
    }
}

void Particle_render(Particle* p, Tessellator* tess, float partialTicks,
                     float cameraX, float cameraY, float cameraZ,
                     float cameraXWithY, float cameraZWithY)
{
    float minU, maxU, minV, maxV;
    if (p->kind == PARTICLE_TERRAIN) {
        // a random quarter of the tile
        minU = ((float)(p->textureId % 16) + p->textureUOffset / 4.0f) / 16.0f;
        maxU = minU + 0.015609375f;
        minV = ((float)(p->textureId / 16) + p->textureVOffset / 4.0f) / 16.0f;
        maxV = minV + 0.015609375f;
    } else {
        minU = (float)(p->textureId % 16) / 16.0f;
        maxU = minU + 0.0624375f;
        minV = (float)(p->textureId / 16) / 16.0f;
        maxV = minV + 0.0624375f;
    }

    float x = p->base.prevX + (p->base.x - p->base.prevX) * partialTicks;
    float y = p->base.prevY + (p->base.y - p->base.prevY) * partialTicks;
    float z = p->base.prevZ + (p->base.z - p->base.prevZ) * partialTicks;
    float s = 0.1f * p->size;

    float br = Entity_getBrightness(&p->base);
    Tessellator_color(tess, p->r * br, p->g * br, p->b * br);
    Tessellator_vertexUV(tess, x - cameraX * s - cameraXWithY * s, y - cameraY * s, z - cameraZ * s - cameraZWithY * s, minU, maxV);
    Tessellator_vertexUV(tess, x - cameraX * s + cameraXWithY * s, y + cameraY * s, z - cameraZ * s + cameraZWithY * s, minU, minV);
    Tessellator_vertexUV(tess, x + cameraX * s + cameraXWithY * s, y + cameraY * s, z + cameraZ * s + cameraZWithY * s, maxU, minV);
    Tessellator_vertexUV(tess, x + cameraX * s - cameraXWithY * s, y - cameraY * s, z + cameraZ * s - cameraZWithY * s, maxU, maxV);
}
