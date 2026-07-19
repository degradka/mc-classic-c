// item/arrow.c: fired projectile entity, see item/arrow.h

#include "arrow.h"
#include "../mob.h"
#include "../renderer/textures.h"
#include "../renderer/tessellator.h"
#include <GL/glew.h>
#include <math.h>
#include <stdlib.h>

static Tessellator sTess;

// implemented in minecraft.c: linear scan of mobs[] for the first one
// (other than owner) overlapping box, matching the real source's own
// level.blockMap spatial query narrowed to isShootable entities (every Mob)
extern bool Minecraft_findArrowTarget(const AABB* box, const Entity* owner, Entity** outHit);

void Arrow_init(Arrow* a, Level* level, Entity* owner, float x, float y, float z, float yaw, float pitch) {
    Entity* e = &a->e;
    Entity_init(e, level);
    a->owner = owner;
    a->hasHit = false;
    a->stickTime = 0;
    a->time = 0;
    e->killCredit = owner;

    Entity_setSize(e, 0.25f, 0.5f);
    e->heightOffset = 0.25f;
    e->makeStepSound = false;

    float f7 = cosf((-yaw) * (float)M_PI / 180.0f - (float)M_PI);
    float f8 = sinf((-yaw) * (float)M_PI / 180.0f - (float)M_PI);
    float cp = cosf((-pitch) * (float)M_PI / 180.0f);
    float sp = sinf((-pitch) * (float)M_PI / 180.0f);
    const float speed = 0.8f;

    // c0.24_st_03: real source's own quirk, confirmed via bytecode not
    // just decompile: prevX/prevZ are set to a small offset near the world
    // origin (Entity's own base constructor never moves them off 0,0,0),
    // not anywhere near the actual spawn point set by Entity_setPosition
    // below, which never touches them either. A freshly fired arrow's very
    // first rendered frame (before its own first tick) genuinely does
    // interpolate from near the origin to its real spawn position. Kept
    // faithfully rather than "fixed", since it plausibly reads as a brief
    // motion streak in practice rather than a jarring glitch
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

void Arrow_onTick(Arrow* a) {
    Entity* e = &a->e;
    a->time++;
    e->prevXRotation = e->xRotation;
    e->prevYRotation = e->yRotation;
    e->prevX = e->x;
    e->prevY = e->y;
    e->prevZ = e->z;

    if (a->hasHit) {
        a->stickTime++;
        if (a->stickTime >= 20) Entity_remove(e);
        return;
    }

    e->motionX *= 0.992f;
    e->motionY *= 0.992f;
    e->motionZ *= 0.992f;
    e->motionY -= 0.02f;

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

        // c0.24_st_03: only within the first 20 ticks of flight can an
        // arrow still register an entity hit (matches the real source's
        // own `this.time > 20` skip, still travels/embeds in blocks after)
        if (!collision && a->time <= 20) {
            Entity* target = NULL;
            if (Minecraft_findArrowTarget(&expanded, e, &target)) {
                Mob_hurt(target, e, 3);
                collision = true;
                a->stickTime = 20;
            }
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

    const float u1 = 0.5f, v1 = 0.15625f, s = 0.05625f;
    glScalef(s, s, s);
    for (int i = 0; i < 4; i++) {
        glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
        glNormal3f(0.0f, -s, 0.0f);
        Tessellator_begin(&sTess);
        Tessellator_vertexUV(&sTess, -8.0f, -2.0f, 0.0f, 0.0f, 0.0f);
        Tessellator_vertexUV(&sTess,  8.0f, -2.0f, 0.0f, u1,   0.0f);
        Tessellator_vertexUV(&sTess,  8.0f,  2.0f, 0.0f, u1,   v1);
        Tessellator_vertexUV(&sTess, -8.0f,  2.0f, 0.0f, 0.0f, v1);
        Tessellator_end(&sTess);
    }

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glPopMatrix();
    glDisable(GL_TEXTURE_2D);
}
