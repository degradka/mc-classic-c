// character/quadruped_model.c

#include "quadruped_model.h"
#include "../math_helper.h"

void QuadrupedModel_init(QuadrupedModel* m, int n) {
    Cube_init(&m->head, 0, 0);
    Cube_addBox(&m->head, -4, -4, -8, 8, 8, 8);
    Cube_setPos(&m->head, 0, (float)(18 - n), -6);
    Cube_init(&m->body, 28, 8);
    Cube_addBox(&m->body, -5, -10, -7, 10, 16, 8);
    Cube_setPos(&m->body, 0, (float)(17 - n), 2);
    Cube* legs[4] = { &m->legBackLeft, &m->legBackRight, &m->legFrontLeft, &m->legFrontRight };
    const float lx[4] = { -3, 3, -3, 3 }, lz[4] = { 7, 7, -5, -5 };
    for (int i = 0; i < 4; ++i) {
        Cube_init(legs[i], 0, 16);
        Cube_addBox(legs[i], -2, 0, -2, 4, n, 4);
        Cube_setPos(legs[i], lx[i], (float)(24 - n), lz[i]);
    }
}

void SheepModel_init(QuadrupedModel* m) {
    QuadrupedModel_init(m, 12);
    Cube_init(&m->head, 0, 0);
    Cube_addBox(&m->head, -3, -4, -6, 6, 6, 8);
    Cube_setPos(&m->head, 0, 6, -8);
    Cube_init(&m->body, 28, 8);
    Cube_addBox(&m->body, -4, -10, -7, 8, 16, 6);
    Cube_setPos(&m->body, 0, 5, 2);
}

void SheepFurModel_init(QuadrupedModel* m) {
    QuadrupedModel_init(m, 12);
    Cube_init(&m->head, 0, 0);
    Cube_addBoxGrown(&m->head, -3, -4, -4, 6, 6, 6, 0.6f);
    Cube_setPos(&m->head, 0, 6, -8);
    Cube_init(&m->body, 28, 8);
    Cube_addBoxGrown(&m->body, -4, -10, -7, 8, 16, 6, 1.75f);
    Cube_setPos(&m->body, 0, 5, 2);
    Cube* legs[4] = { &m->legBackLeft, &m->legBackRight, &m->legFrontLeft, &m->legFrontRight };
    const float lx[4] = { -3, 3, -3, 3 }, lz[4] = { 7, 7, -5, -5 };
    for (int i = 0; i < 4; ++i) {
        Cube_init(legs[i], 0, 16);
        Cube_addBoxGrown(legs[i], -2, 0, -2, 4, 6, 4, 0.5f);
        Cube_setPos(legs[i], lx[i], 12, lz[i]);
    }
}

void QuadrupedModel_render(QuadrupedModel* m, float animStep, float run, float headYaw, float headPitch) {
    const float pi = 3.1415927f;
    m->head.yRot = headYaw / 57.295776f;
    m->head.xRot = headPitch / 57.295776f;
    m->body.xRot = 1.5707964f;
    m->legBackLeft.xRot   = MathHelper_cos(animStep * 0.6662f) * 1.4f * run;
    m->legBackRight.xRot  = MathHelper_cos(animStep * 0.6662f + pi) * 1.4f * run;
    m->legFrontLeft.xRot  = MathHelper_cos(animStep * 0.6662f + pi) * 1.4f * run;
    m->legFrontRight.xRot = MathHelper_cos(animStep * 0.6662f) * 1.4f * run;
    Cube_render(&m->head);
    Cube_render(&m->body);
    Cube_render(&m->legBackLeft);
    Cube_render(&m->legBackRight);
    Cube_render(&m->legFrontLeft);
    Cube_render(&m->legFrontRight);
}
