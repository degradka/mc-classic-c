// character/mob_skeleton_model.c

#include "../math_helper.h"
#include "mob_skeleton_model.h"
#include "../common.h"
#include <math.h>

void MobSkeletonModel_init(MobSkeletonModel* m) {
    Cube_init(&m->head, 0, 0);
    Cube_addBox(&m->head, -4, -8, -4, 8, 8, 8);

    Cube_init(&m->body, 16, 16);
    Cube_addBox(&m->body, -4, 0, -2, 8, 12, 4);

    Cube_init(&m->rightArm, 40, 16);
    Cube_addBox(&m->rightArm, -1, -2, -1, 2, 12, 2);
    Cube_setPos(&m->rightArm, -5, 2, 0);

    Cube_init(&m->leftArm, 40, 16);
    m->leftArm.mirror = 1;
    Cube_addBox(&m->leftArm, -1, -2, -1, 2, 12, 2);
    Cube_setPos(&m->leftArm, 5, 2, 0);

    Cube_init(&m->rightLeg, 0, 16);
    Cube_addBox(&m->rightLeg, -1, 0, -1, 2, 12, 2);
    Cube_setPos(&m->rightLeg, -2, 12, 0);

    Cube_init(&m->leftLeg, 0, 16);
    m->leftLeg.mirror = 1;
    Cube_addBox(&m->leftLeg, -1, 0, -1, 2, 12, 2);
    Cube_setPos(&m->leftLeg, 2, 12, 0);
}

void MobSkeletonModel_render(MobSkeletonModel* m, float animStep, float run, float age,
                             float headYaw, float headPitch, float attackSwing) {
    const float pi = 3.1415927f;
    m->head.yRot = headYaw   / 57.295776f;
    m->head.xRot = headPitch / 57.295776f;

    float f2 = MathHelper_sin(attackSwing * pi);
    float f3 = MathHelper_sin((1.0f - (1.0f - attackSwing) * (1.0f - attackSwing)) * pi);
    m->rightArm.zRot = 0.0f;
    m->leftArm.zRot = 0.0f;
    m->rightArm.yRot = -(0.1f - f2 * 0.6f);
    m->leftArm.yRot  =  0.1f - f2 * 0.6f;
    m->rightArm.xRot = -1.5707964f;
    m->leftArm.xRot  = -1.5707964f;
    m->rightArm.xRot -= f2 * 1.2f - f3 * 0.4f;
    m->leftArm.xRot  -= f2 * 1.2f - f3 * 0.4f;
    m->rightArm.zRot += MathHelper_cos(age * 0.09f) * 0.05f + 0.05f;
    m->leftArm.zRot  -= MathHelper_cos(age * 0.09f) * 0.05f + 0.05f;
    m->rightArm.xRot += MathHelper_sin(age * 0.067f) * 0.05f;
    m->leftArm.xRot  -= MathHelper_sin(age * 0.067f) * 0.05f;

    m->rightLeg.xRot = MathHelper_cos(animStep * 0.6662f) * 1.4f * run;
    m->leftLeg.xRot  = MathHelper_cos(animStep * 0.6662f + pi) * 1.4f * run;

    Cube_render(&m->head);
    Cube_render(&m->body);
    Cube_render(&m->rightArm);
    Cube_render(&m->leftArm);
    Cube_render(&m->rightLeg);
    Cube_render(&m->leftLeg);
}
