// item/arrow.c

#include "arrow.h"
#include "../mob.h"
#include "../renderer/textures.h"
#include "../renderer/tessellator.h"
#include <GL/glew.h>
#include <math.h>
#include <stdlib.h>

static Tessellator sTess;

static inline float frand01(void) { return (float)rand() / (float)RAND_MAX; }

extern bool Minecraft_findArrowTarget(const AABB* box, const Entity* owner, int flightTime, Entity** outHit);

void Arrow_init(Arrow* a, Level* level, Entity* owner, float x, float y, float z, float yaw, float pitch, float speed) {
    Entity* e = &a->e;
    Entity_init(e, level);
    a->owner = owner;
    a->hasHit = false;
    a->stickTime = 0;
    a->time = 0;
    e->killCredit = owner;

    a->pickedUp = false;
    a->pickupTime = 0;
    a->pickupTarget = NULL;

    a->damage = 3;
    a->type = 0;
    if (owner && owner->aiClassTag == AI_CLASS_PLAYER) {
        a->damage = 7;
    } else {
        a->type = 1;
    }

    Entity_setSize(e, 0.3f, 0.5f);
    e->heightOffset = 0.25f;
    e->makeStepSound = false;

    float f7 = cosf((-yaw) * (float)M_PI / 180.0f - (float)M_PI);
    float f8 = sinf((-yaw) * (float)M_PI / 180.0f - (float)M_PI);
    float cp = cosf((-pitch) * (float)M_PI / 180.0f);
    float sp = sinf((-pitch) * (float)M_PI / 180.0f);
    a->gravity = 1.0f / speed;

    // xo and zo start near the origin, the first frame streaks in
    e->prevX -= f7 * 0.2f;
    e->prevZ += f8 * 0.2f;

    e->motionX = f8 * cp * speed;
    e->motionY = sp * speed;
    e->motionZ = f7 * cp * speed;

    Entity_setPosition(e, x - f7 * 0.2f, y, z + f8 * 0.2f);

    float horiz = sqrtf(e->motionX * e->motionX + e->motionZ * e->motionZ);
    e->yRotation = e->prevYRotation = (float)(atan2(e->motionX, e->motionZ) * 180.0 / M_PI);
    e->xRotation = e->prevXRotation = (float)(atan2(e->motionY, horiz) * 180.0 / M_PI);
}

void Arrow_startPickup(Arrow* a, Entity* target) {
    a->pickedUp = true;
    a->pickupTime = 0;
    a->pickupOrgX = a->e.x;
    a->pickupOrgY = a->e.y;
    a->pickupOrgZ = a->e.z;
    a->pickupTarget = target;
}

void Arrow_onTick(Arrow* a) {
    Entity* e = &a->e;

    if (a->pickedUp) {
        a->pickupTime++;
        if (a->pickupTime >= 3) {
            Entity_remove(e);
            return;
        }
        float f2 = (float)a->pickupTime / 3.0f;
        f2 *= f2;
        e->prevX = e->x;
        e->prevY = e->y;
        e->prevZ = e->z;
        e->x = a->pickupOrgX + (a->pickupTarget->x - a->pickupOrgX) * f2;
        e->y = a->pickupOrgY + (a->pickupTarget->y - 1.0f - a->pickupOrgY) * f2;
        e->z = a->pickupOrgZ + (a->pickupTarget->z - a->pickupOrgZ) * f2;
        return;
    }

    a->time++;
    e->prevXRotation = e->xRotation;
    e->prevYRotation = e->yRotation;
    e->prevX = e->x;
    e->prevY = e->y;
    e->prevZ = e->z;

    if (a->hasHit) {
        a->stickTime++;
        // type 0 has a 1 in 100 chance a tick to vanish after 300 stuck ticks
        // type 1 goes after 20
        if (a->type == 0) {
            if (a->stickTime >= 300 && frand01() < 0.01f) Entity_remove(e);
        } else if (a->stickTime >= 20) {
            Entity_remove(e);
        }
        return;
    }

    e->motionX *= 0.998f;
    e->motionY *= 0.998f;
    e->motionZ *= 0.998f;
    e->motionY -= 0.02f * a->gravity;

    float speed = sqrtf(e->motionX * e->motionX + e->motionY * e->motionY + e->motionZ * e->motionZ);
    int steps = (int)(speed / 0.2f + 1.0f);
    float stepX = e->motionX / (float)steps;
    float stepY = e->motionY / (float)steps;
    float stepZ = e->motionZ / (float)steps;

    bool collision = false;
    for (int i = 0; i < steps && !collision; i++) {
        AABB expanded = AABB_expand(&e->boundingBox, stepX, stepY, stepZ);

        ArrayList_AABB hits = Level_getCubes(e->level, &expanded);
        if (hits.size > 0) collision = true;
        if (hits.aabbs) free(hits.aabbs);

        // hitting an entity removes the arrow right away
        Entity* target = NULL;
        if (Minecraft_findArrowTarget(&expanded, a->owner, a->time, &target)) {
            Mob_hurt(target, e, a->damage);
            Entity_remove(e);
            return;
        }

        if (collision) continue;
        AABB_move(&e->boundingBox, stepX, stepY, stepZ);
        e->x += stepX;
        e->y += stepY;
        e->z += stepZ;
    }

    if (collision) {
        a->hasHit = true;
        e->motionX = e->motionY = e->motionZ = 0.0f;
    }

    if (!a->hasHit) {
        float horiz = sqrtf(e->motionX * e->motionX + e->motionZ * e->motionZ);
        e->yRotation = (float)(atan2(e->motionX, e->motionZ) * 180.0 / M_PI);
        e->xRotation = (float)(atan2(e->motionY, horiz) * 180.0 / M_PI);
        while (e->xRotation - e->prevXRotation < -180.0f) e->prevXRotation -= 360.0f;
        while (e->xRotation - e->prevXRotation >= 180.0f) e->prevXRotation += 360.0f;
        while (e->yRotation - e->prevYRotation < -180.0f) e->prevYRotation -= 360.0f;
        while (e->yRotation - e->prevYRotation >= 180.0f) e->prevYRotation += 360.0f;
    }
}

void Arrow_render(const Arrow* a, float partialTicks) {
    const Entity* e = &a->e;
    static int tex = 0;
    if (!tex) tex = loadTexture("resources/item/arrows.png", GL_NEAREST);

    float brightness = Entity_getBrightness(e);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPushMatrix();
    glColor4f(brightness, brightness, brightness, 1.0f);

    float ix = e->prevX + (e->x - e->prevX) * partialTicks;
    float iy = e->prevY + (e->y - e->prevY) * partialTicks - e->heightOffset / 2.0f;
    float iz = e->prevZ + (e->z - e->prevZ) * partialTicks;
    glTranslatef(ix, iy, iz);

    float yaw   = e->prevYRotation + (e->yRotation - e->prevYRotation) * partialTicks;
    float pitch = e->prevXRotation + (e->xRotation - e->prevXRotation) * partialTicks;
    glRotatef(yaw - 90.0f, 0.0f, 1.0f, 0.0f);
    glRotatef(pitch, 0.0f, 0.0f, 1.0f);
    glRotatef(45.0f, 1.0f, 0.0f, 0.0f);

    // type 1 uses the band below type 0 in arrows.png
    const float s = 0.05625f;
    const float shaftV0 = (float)(5 + a->type * 10) / 32.0f;
    const float shaftU1 = 0.15625f;
    const float shaftV1 = (float)(10 + a->type * 10) / 32.0f;
    glScalef(s, s, s);
    glNormal3f(s, 0.0f, 0.0f);
    Tessellator_begin(&sTess);
    Tessellator_vertexUV(&sTess, -7.0f, -2.0f, -2.0f, 0.0f,     shaftV0);
    Tessellator_vertexUV(&sTess, -7.0f, -2.0f,  2.0f, shaftU1,  shaftV0);
    Tessellator_vertexUV(&sTess, -7.0f,  2.0f,  2.0f, shaftU1,  shaftV1);
    Tessellator_vertexUV(&sTess, -7.0f,  2.0f, -2.0f, 0.0f,     shaftV1);
    Tessellator_end(&sTess);

    glNormal3f(-s, 0.0f, 0.0f);
    Tessellator_begin(&sTess);
    Tessellator_vertexUV(&sTess, -7.0f,  2.0f, -2.0f, 0.0f,     shaftV0);
    Tessellator_vertexUV(&sTess, -7.0f,  2.0f,  2.0f, shaftU1,  shaftV0);
    Tessellator_vertexUV(&sTess, -7.0f, -2.0f,  2.0f, shaftU1,  shaftV1);
    Tessellator_vertexUV(&sTess, -7.0f, -2.0f, -2.0f, 0.0f,     shaftV1);
    Tessellator_end(&sTess);

    const float u1 = 0.5f;
    const float v0 = (float)(0 + a->type * 10) / 32.0f;
    const float v1 = (float)(5 + a->type * 10) / 32.0f;
    for (int i = 0; i < 4; i++) {
        glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
        glNormal3f(0.0f, -s, 0.0f);
        Tessellator_begin(&sTess);
        Tessellator_vertexUV(&sTess, -8.0f, -2.0f, 0.0f, 0.0f, v0);
        Tessellator_vertexUV(&sTess,  8.0f, -2.0f, 0.0f, u1,   v0);
        Tessellator_vertexUV(&sTess,  8.0f,  2.0f, 0.0f, u1,   v1);
        Tessellator_vertexUV(&sTess, -8.0f,  2.0f, 0.0f, 0.0f, v1);
        Tessellator_end(&sTess);
    }

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glPopMatrix();
    glDisable(GL_TEXTURE_2D);
}
