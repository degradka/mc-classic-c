// mob_zombie_model.c: the real AI Zombie's own model, see mob_zombie_model.h

#include "mob_zombie_model.h"
#include "../common.h"
#include <math.h>

void MobZombieModel_init(MobZombieModel* m) {
    Cube_init(&m->head, 0, 0);
    Cube_addBox(&m->head, -4, -8, -4, 8, 8, 8);

    Cube_init(&m->body, 16, 16);
    Cube_addBox(&m->body, -4, 0, -2, 8, 12, 4);

    Cube_init(&m->rightArm, 40, 16);
    Cube_addBox(&m->rightArm, -3, -2, -2, 4, 12, 4);
    Cube_setPos(&m->rightArm, -5, 2, 0);

    Cube_init(&m->leftArm, 40, 16);
    Cube_addBox(&m->leftArm, -1, -2, -2, 4, 12, 4);
    Cube_setPos(&m->leftArm, 5, 2, 0);

    Cube_init(&m->rightLeg, 0, 16);
    Cube_addBox(&m->rightLeg, -2, 0, -2, 4, 12, 4);
    Cube_setPos(&m->rightLeg, -2, 12, 0);

    Cube_init(&m->leftLeg, 0, 16);
    Cube_addBox(&m->leftLeg, -2, 0, -2, 4, 12, 4);
    Cube_setPos(&m->leftLeg, 2, 12, 0);
}

void MobZombieModel_render(MobZombieModel* m, float animStep, float run, float age,
                           float headYaw, float headPitch, float attackSwing) {
    m->head.yRot = headYaw   / 57.29578f;
    m->head.xRot = headPitch / 57.29578f;

    // c0.24_st_03: real Zombie pose, arms locked forward instead of swinging
    // while walking; the only arm motion is a punch swing driven by
    // attackSwing (0..1 over the attack animation) plus a small idle sway
    float f2 = (float)sin(attackSwing * M_PI);
    float oneMinus = 1.0f - attackSwing;
    float f3 = (float)sin((1.0f - oneMinus * oneMinus) * M_PI);

    m->rightArm.yRot = -(0.1f - f2 * 0.6f);
    m->leftArm.yRot  =  (0.1f - f2 * 0.6f);
    m->rightArm.xRot = -1.5707964f - f2 * 1.2f + f3 * 0.4f;
    m->leftArm.xRot  = -1.5707964f - f2 * 1.2f + f3 * 0.4f;
    m->rightArm.zRot = (float)cos(age * 0.09) * 0.05f + 0.05f;
    m->leftArm.zRot  = -((float)cos(age * 0.09) * 0.05f + 0.05f);
    m->rightArm.xRot += (float)sin(age * 0.067) * 0.05f;
    m->leftArm.xRot  -= (float)sin(age * 0.067) * 0.05f;

    // legs keep the base humanoid walk swing, unaffected by the arm override
    m->rightLeg.xRot = (float)cos(animStep * 0.6662) * 1.4f * run;
    m->leftLeg.xRot  = (float)cos(animStep * 0.6662 + M_PI) * 1.4f * run;

    Cube_render(&m->head);
    Cube_render(&m->body);
    Cube_render(&m->rightArm);
    Cube_render(&m->leftArm);
    Cube_render(&m->rightLeg);
    Cube_render(&m->leftLeg);
}
