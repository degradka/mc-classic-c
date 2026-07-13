// character/zombie_model.h: the plain humanoid model, network players use it

#ifndef ZOMBIE_MODEL_H
#define ZOMBIE_MODEL_H

#include "cube.h"

typedef struct {
    Cube head;
    Cube body;
    Cube rightArm, leftArm;
    Cube rightLeg, leftLeg;
} ZombieModel;

void ZombieModel_init  (ZombieModel* m);
void ZombieModel_render(const ZombieModel* m, float time, float swing, float idle, float headYRot, float headXRot, float scale);

#endif