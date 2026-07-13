// item/item.c

#include "item.h"
#include "../renderer/textures.h"
#include "../renderer/tessellator.h"
#include "../level/tile/tile.h"
#include <GL/glew.h>
#include <math.h>
#include <stdlib.h>

static Tessellator sTess;

void Item_init(Item* it, Level* level, float x, float y, float z, int resource) {
    Entity* e = &it->e;
    Entity_init(e, level);
    Entity_setSize(e, 0.25f, 0.25f);
    e->heightOffset = 0.125f; // bbHeight/2
    Entity_setPosition(e, x, y, z);
    it->resource = resource;
    it->rot = (float)rand() / (float)RAND_MAX * 360.0f;
    e->motionX = (float)rand() / (float)RAND_MAX * 0.2f - 0.1f;
    e->motionY = 0.2f;
    e->motionZ = (float)rand() / (float)RAND_MAX * 0.2f - 0.1f;
    it->tickCount = 0;
    it->age = 0;
    it->pickedUp = false;
    e->makeStepSound = false;
}

void Item_startPickup(Item* it, Entity* target) {
    it->pickedUp = true;
    it->pickupTime = 0;
    it->pickupOrgX = it->e.x;
    it->pickupOrgY = it->e.y;
    it->pickupOrgZ = it->e.z;
    it->pickupTarget = target;
}

void Item_onTick(Item* it) {
    Entity* e = &it->e;

    // TakeItemAnim.tick
    if (it->pickedUp) {
        it->pickupTime++;
        if (it->pickupTime >= 3) {
            Entity_remove(e);
            return;
        }
        float f2 = (float)it->pickupTime / 3.0f;
        f2 *= f2;
        e->prevX = e->x;
        e->prevY = e->y;
        e->prevZ = e->z;
        e->x = it->pickupOrgX + (it->pickupTarget->x - it->pickupOrgX) * f2;
        e->y = it->pickupOrgY + (it->pickupTarget->y - 1.0f - it->pickupOrgY) * f2;
        e->z = it->pickupOrgZ + (it->pickupTarget->z - it->pickupOrgZ) * f2;
        return;
    }

    e->prevX = e->x;
    e->prevY = e->y;
    e->prevZ = e->z;

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

    it->tickCount++;
    it->age++;
    if (it->age >= 6000) Entity_remove(e);
}

static void renderIconCube(int resource) {
    const Tile* tile = (resource > 0 && resource < 256) ? gTiles[resource] : NULL;
    int texId = tile ? tile->textureId : resource;
    float col = (float)(texId % 16);
    float row = (float)(texId / 16);
    float u0 = (col + 0.25f) / 16.0f, u1 = (col + 0.75f) / 16.0f;
    float v0 = (row + 0.25f) / 16.0f, v1 = (row + 0.75f) / 16.0f;

    const float n = 2.0f; // half extent, box spans -2..2

    // -Z, +Z, -X, +X, -Y, +Y faces, each a single quad
    Tessellator_begin(&sTess);
    Tessellator_vertexUV(&sTess, -n, -n, -n, u0, v0);
    Tessellator_vertexUV(&sTess,  n, -n, -n, u1, v0);
    Tessellator_vertexUV(&sTess,  n,  n, -n, u1, v1);
    Tessellator_vertexUV(&sTess, -n,  n, -n, u0, v1);
    Tessellator_end(&sTess);

    Tessellator_begin(&sTess);
    Tessellator_vertexUV(&sTess,  n, -n,  n, u0, v0);
    Tessellator_vertexUV(&sTess, -n, -n,  n, u1, v0);
    Tessellator_vertexUV(&sTess, -n,  n,  n, u1, v1);
    Tessellator_vertexUV(&sTess,  n,  n,  n, u0, v1);
    Tessellator_end(&sTess);

    Tessellator_begin(&sTess);
    Tessellator_vertexUV(&sTess, -n, -n,  n, u0, v0);
    Tessellator_vertexUV(&sTess, -n, -n, -n, u1, v0);
    Tessellator_vertexUV(&sTess, -n,  n, -n, u1, v1);
    Tessellator_vertexUV(&sTess, -n,  n,  n, u0, v1);
    Tessellator_end(&sTess);

    Tessellator_begin(&sTess);
    Tessellator_vertexUV(&sTess,  n, -n, -n, u0, v0);
    Tessellator_vertexUV(&sTess,  n, -n,  n, u1, v0);
    Tessellator_vertexUV(&sTess,  n,  n,  n, u1, v1);
    Tessellator_vertexUV(&sTess,  n,  n, -n, u0, v1);
    Tessellator_end(&sTess);

    Tessellator_begin(&sTess);
    Tessellator_vertexUV(&sTess, -n, -n,  n, u0, v0);
    Tessellator_vertexUV(&sTess,  n, -n,  n, u1, v0);
    Tessellator_vertexUV(&sTess,  n, -n, -n, u1, v1);
    Tessellator_vertexUV(&sTess, -n, -n, -n, u0, v1);
    Tessellator_end(&sTess);

    Tessellator_begin(&sTess);
    Tessellator_vertexUV(&sTess, -n,  n, -n, u0, v0);
    Tessellator_vertexUV(&sTess,  n,  n, -n, u1, v0);
    Tessellator_vertexUV(&sTess,  n,  n,  n, u1, v1);
    Tessellator_vertexUV(&sTess, -n,  n,  n, u0, v1);
    Tessellator_end(&sTess);
}

void Item_render(const Item* it, float partialTicks) {
    const Entity* e = &it->e;

    float brightness = Level_getBrightness(e->level, (int)e->x, (int)e->y, (int)e->z);
    float spin = it->rot + ((float)it->tickCount + partialTicks) * 3.0f;

    glPushMatrix();
    glColor4f(brightness, brightness, brightness, 1.0f);

    float bob = (float)sin(spin / 10.0) * 0.1f + 0.1f;
    float ix = e->prevX + (e->x - e->prevX) * partialTicks;
    float iy = e->prevY + (e->y - e->prevY) * partialTicks + bob;
    float iz = e->prevZ + (e->z - e->prevZ) * partialTicks;
    glTranslatef(ix, iy, iz);
    glRotatef(spin, 0.0f, 1.0f, 0.0f);
    glScalef(0.0625f, 0.0625f, 0.0625f);

    static int tex = 0;
    if (!tex) tex = loadTexture("resources/terrain.png", GL_NEAREST);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    renderIconCube(it->resource);
    glDisable(GL_TEXTURE_2D);

    // additive glow pulse pass, same geometry, untextured
    float pulse = (float)sin(spin / 10.0) * 0.5f + 0.5f;
    pulse *= pulse;
    pulse *= pulse;
    glColor4f(1.0f, 1.0f, 1.0f, pulse * 0.4f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDisable(GL_ALPHA_TEST);
    renderIconCube(it->resource);
    glEnable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glPopMatrix();
}
