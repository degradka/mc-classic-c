// item/item.c

#include "item.h"
#include "../jrandom.h"
#include "../renderer/textures.h"
#include "../level/tile/tile.h"
#include <GL/glew.h>
#include <math.h>
#include <stdlib.h>

// the shared terrain.png, animated by the water and lava texture fx
extern int Minecraft_getTerrainTexture(void);

void Item_init(Item* it, Level* level, float x, float y, float z, int resource) {
    Entity* e = &it->e;
    Entity_init(e, level);
    Entity_setSize(e, 0.25f, 0.25f);
    e->heightOffset = 0.125f; // bbHeight/2
    Entity_setPosition(e, x, y, z);
    it->resource = resource;
    it->rot = (float)(JRandom_mathRandom() * 360.0);
    e->motionX = (float)(JRandom_mathRandom() * (double)0.2f - (double)0.1f);
    e->motionY = 0.2f;
    e->motionZ = (float)(JRandom_mathRandom() * (double)0.2f - (double)0.1f);
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
    float xd = e->motionX, yd = e->motionY, zd = e->motionZ;
    Entity_move(e, xd, yd, zd);
    e->motionX = xd;
    e->motionY = yd;
    e->motionZ = zd;
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

// ItemModel: a 4 pixel cube using the middle half of the tile's cell, built once per texture
static void emitItemQuad(const float* a, const float* b, const float* c, const float* d,
                         float uLo, float uHi, float vLo, float vHi) {
    const float* v[4] = { a, b, c, d };
    const float uv[4][2] = { { uLo, vHi }, { uHi, vHi }, { uHi, vLo }, { uLo, vLo } };
    float e1[3], e2[3], n[3];
    for (int k = 0; k < 3; ++k) {
        e1[k] = v[1][k] - v[0][k];
        e2[k] = v[1][k] - v[2][k];
    }
    float l1 = sqrtf(e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2]);
    float l2 = sqrtf(e2[0] * e2[0] + e2[1] * e2[1] + e2[2] * e2[2]);
    for (int k = 0; k < 3; ++k) { e1[k] /= l1; e2[k] /= l2; }
    n[0] = e1[1] * e2[2] - e1[2] * e2[1];
    n[1] = e1[2] * e2[0] - e1[0] * e2[2];
    n[2] = e1[0] * e2[1] - e1[1] * e2[0];
    float ln = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    glNormal3f(n[0] / ln, n[1] / ln, n[2] / ln);
    for (int k = 0; k < 4; ++k) {
        glTexCoord2f(uv[k][0], uv[k][1]);
        glVertex3f(v[k][0] * 0.0625f, v[k][1] * 0.0625f, v[k][2] * 0.0625f);
    }
}

static void renderIconCube(int resource) {
    // Item builds its models from the tile's texture W, not the tile id
    const Tile* tile = (resource > 0 && resource < 256) ? gTiles[resource] : NULL;
    int texId = tile ? tile->textureId : resource;
    static unsigned int lists[256];
    if (texId < 0 || texId > 255) return;
    if (!lists[texId]) {
        float uHi = ((float)(texId % 16) + (1.0f - 0.25f)) / 16.0f;
        float vHi = ((float)(texId / 16) + (1.0f - 0.25f)) / 16.0f;
        float uLo = ((float)(texId % 16) + 0.25f) / 16.0f;
        float vLo = ((float)(texId / 16) + 0.25f) / 16.0f;
        const float v1[3] = { -2, -2, -2 }, v2[3] = { 2, -2, -2 }, v3[3] = { 2, 2, -2 }, v4[3] = { -2, 2, -2 };
        const float v5[3] = { -2, -2,  2 }, v6[3] = { 2, -2,  2 }, v7[3] = { 2, 2,  2 }, v8[3] = { -2, 2,  2 };
        lists[texId] = glGenLists(1);
        glNewList(lists[texId], GL_COMPILE);
        glBegin(GL_QUADS);
        emitItemQuad(v6, v2, v3, v7, uLo, uHi, vLo, vHi);
        emitItemQuad(v1, v5, v8, v4, uLo, uHi, vLo, vHi);
        emitItemQuad(v6, v5, v1, v2, uLo, uHi, vLo, vHi);
        emitItemQuad(v3, v4, v8, v7, uLo, uHi, vLo, vHi);
        emitItemQuad(v2, v1, v4, v3, uLo, uHi, vLo, vHi);
        emitItemQuad(v5, v6, v7, v8, uLo, uHi, vLo, vHi);
        glEnd();
        glEndList();
    }
    glCallList(lists[texId]);
}

void Item_render(const Item* it, float partialTicks) {
    const Entity* e = &it->e;

    glBindTexture(GL_TEXTURE_2D, Minecraft_getTerrainTexture());
    float brightness = Level_getBrightness(e->level, (int)e->x, (int)e->y, (int)e->z);
    float spin = it->rot + ((float)it->tickCount + partialTicks) * 3.0f;

    glPushMatrix();
    glColor4f(brightness, brightness, brightness, 1.0f);

    float wave = (float)sin((double)(spin / 10.0f));
    float bob = wave * 0.1f + 0.1f;
    float ix = e->prevX + (e->x - e->prevX) * partialTicks;
    float iy = e->prevY + (e->y - e->prevY) * partialTicks + bob;
    float iz = e->prevZ + (e->z - e->prevZ) * partialTicks;
    glTranslatef(ix, iy, iz);
    glRotatef(spin, 0.0f, 1.0f, 0.0f);

    renderIconCube(it->resource);

    // additive glow pulse pass, same geometry, untextured
    float pulse = wave * 0.5f + 0.5f;
    pulse *= pulse;
    pulse *= pulse;
    glColor4f(1.0f, 1.0f, 1.0f, pulse * 0.4f);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDisable(GL_ALPHA_TEST);
    renderIconCube(it->resource);
    glEnable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glPopMatrix();
    glEnable(GL_TEXTURE_2D);
}
