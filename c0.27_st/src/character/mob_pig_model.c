// mob_pig_model.c: Pig's model, see mob_pig_model.h

#include "mob_pig_model.h"
#include "../common.h"
#include <math.h>

void MobPigModel_init(MobPigModel* m) {
    Cube_init(&m->head, 0, 0);
    Cube_addBox(&m->head, -4, -4, -8, 8, 8, 8);
    Cube_setPos(&m->head, 0, 12, -6);

    // torso is authored standing up, then rotated flat below (xRot fixed at
    // 90 degrees), matching the real source's own build-vertical-then-tip
    // technique for its body box
    Cube_init(&m->body, 28, 8);
    Cube_addBox(&m->body, -5, -10, -7, 10, 16, 8);
    Cube_setPos(&m->body, 0, 11, 2);
    m->body.xRot = 1.5707963f;

    Cube_init(&m->legFrontLeft, 0, 16);
    Cube_addBox(&m->legFrontLeft, -2, 0, -2, 4, 6, 4);
    Cube_setPos(&m->legFrontLeft, -3, 18, -5);

    Cube_init(&m->legFrontRight, 0, 16);
    Cube_addBox(&m->legFrontRight, -2, 0, -2, 4, 6, 4);
    Cube_setPos(&m->legFrontRight, 3, 18, -5);

    Cube_init(&m->legBackLeft, 0, 16);
    Cube_addBox(&m->legBackLeft, -2, 0, -2, 4, 6, 4);
    Cube_setPos(&m->legBackLeft, -3, 18, 7);

    Cube_init(&m->legBackRight, 0, 16);
    Cube_addBox(&m->legBackRight, -2, 0, -2, 4, 6, 4);
    Cube_setPos(&m->legBackRight, 3, 18, 7);
}

void MobPigModel_render(MobPigModel* m, float animStep, float run, float age,
                        float headYaw, float headPitch) {
    (void)age; // Pig has no arms, so no idle sway term exists for it
    m->head.yRot = headYaw   / 57.29578f;
    m->head.xRot = headPitch / 57.29578f;

    // diagonal quadruped gait: front-right+back-left share one phase,
    // front-left+back-right share the opposite phase
    m->legFrontRight.xRot = (float)cos(animStep * 0.6662) * 1.4f * run;
    m->legBackLeft.xRot   = (float)cos(animStep * 0.6662) * 1.4f * run;
    m->legFrontLeft.xRot  = (float)cos(animStep * 0.6662 + M_PI) * 1.4f * run;
    m->legBackRight.xRot  = (float)cos(animStep * 0.6662 + M_PI) * 1.4f * run;

    Cube_render(&m->head);
    Cube_render(&m->body);
    Cube_render(&m->legFrontLeft);
    Cube_render(&m->legFrontRight);
    Cube_render(&m->legBackLeft);
    Cube_render(&m->legBackRight);
}
