// item/sign.c: a free floating Sign entity, see item/sign.h

#include "sign.h"
#include "sign_model.h"
#include "../renderer/textures.h"
#include "../renderer/tessellator.h"
#include <GL/glew.h>
#include <math.h>

static Tessellator sTess;

// c0.24_st_03: hardcoded, not user-editable in this build - confirmed no
// text-entry path anywhere references Sign at all
static const char* const SIGN_MESSAGES[4] = {
    "This is a test",
    "of the signs.",
    "Each line can",
    "be 15 chars!",
};

void Sign_init(Sign* s, Level* level, float x, float y, float z, float yaw) {
    Entity* e = &s->e;
    Entity_init(e, level);
    Entity_setSize(e, 0.5f, 1.5f);
    Entity_setPosition(e, x, y, z);

    s->rot = -yaw;
    e->heightOffset = 1.5f;
    // c0.24_st_03: gentle toss in the facing direction, matches the real
    // source exactly - not placed exactly where clicked, it falls and
    // settles like a dropped Item (same physics, smaller/fixed velocity)
    e->motionX = -sinf(s->rot * (float)M_PI / 180.0f) * 0.05f;
    e->motionY = 0.2f;
    e->motionZ = -cosf(s->rot * (float)M_PI / 180.0f) * 0.05f;
    e->makeStepSound = false;
}

void Sign_onTick(Sign* s) {
    Entity* e = &s->e;
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
}

void Sign_render(const Sign* s, float partialTicks, Font* font) {
    const Entity* e = &s->e;

    static int tex = 0;
    if (!tex) tex = loadTexture("resources/item/sign.png", GL_NEAREST);
    static SignModel model;
    static bool modelInit = false;
    if (!modelInit) { SignModel_init(&model); modelInit = true; }

    float brightness = Level_getBrightness(e->level, (int)e->x, (int)e->y, (int)e->z);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPushMatrix();
    glColor4f(brightness, brightness, brightness, 1.0f);

    float ix = e->prevX + (e->x - e->prevX) * partialTicks;
    float iy = e->prevY + (e->y - e->prevY) * partialTicks - e->heightOffset / 2.0f;
    float iz = e->prevZ + (e->z - e->prevZ) * partialTicks;
    glTranslatef(ix, iy, iz);
    glRotatef(s->rot, 0.0f, 1.0f, 0.0f);

    glPushMatrix();
    glScalef(1.0f, -1.0f, -1.0f);
    glScalef(0.0625f, 0.0625f, 0.0625f);
    Cube_render(&model.board);
    Cube_render(&model.post);
    glPopMatrix();

    // c0.24_st_03: 4 lines of world-space bitmap text on the board's face,
    // matching the real source's exact position/scale
    glTranslatef(0.0f, 0.5f, 0.09f);
    const float ts = 1.0f / 60.0f;
    glScalef(ts, -ts, ts);
    glEnable(GL_BLEND);
    for (int i = 0; i < 4; i++) {
        int tw = Font_width(font, SIGN_MESSAGES[i]);
        Font_drawShadow(font, &sTess, SIGN_MESSAGES[i], -tw / 2, i * 10 - 4 * 5, 0x202020);
    }
    glDisable(GL_BLEND);

    glDisable(GL_TEXTURE_2D);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glPopMatrix();
}
