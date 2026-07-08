// character/zombie_model.c

#include "zombie_model.h"
#include "../common.h"
#include <math.h>

void ZombieModel_init(ZombieModel* m) {
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

// the head angles come in degrees
// swing scales the walk, idle sways the arms a little
void ZombieModel_render(const ZombieModel* m, float time, float swing, float idle, float headYRot, float headXRot, float scale) {
    Cube* rightArm = (Cube*)&m->rightArm;
    Cube* leftArm  = (Cube*)&m->leftArm;
    ((Cube*)&m->head)->yRot      = headYRot / 57.29578f;
    ((Cube*)&m->head)->xRot      = headXRot / 57.29578f;
    rightArm->xRot = (float)cos((double)time * 0.6662 + M_PI) * 2.0f * swing;
    rightArm->zRot = (float)(cos((double)time * 0.2312) + 1.0) * swing;
    leftArm->xRot  = (float)cos((double)time * 0.6662) * 2.0f * swing;
    leftArm->zRot  = (float)(cos((double)time * 0.2812) - 1.0) * swing;
    ((Cube*)&m->rightLeg)->xRot  = (float)cos((double)time * 0.6662) * 1.4f * swing;
    ((Cube*)&m->leftLeg)->xRot   = (float)cos((double)time * 0.6662 + M_PI) * 1.4f * swing;
    rightArm->zRot += (float)cos((double)idle * 0.09) * 0.05f + 0.05f;
    leftArm->zRot  -= (float)cos((double)idle * 0.09) * 0.05f + 0.05f;
    rightArm->xRot += (float)sin((double)idle * 0.067) * 0.05f;
    leftArm->xRot  -= (float)sin((double)idle * 0.067) * 0.05f;

    Cube_render(&m->head, scale);
    Cube_render(&m->body, scale);
    Cube_render(&m->rightArm, scale);
    Cube_render(&m->leftArm, scale);
    Cube_render(&m->rightLeg, scale);
    Cube_render(&m->leftLeg, scale);
}
