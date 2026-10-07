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
// run scales the swing, age adds a small idle arm sway
void ZombieModel_render(ZombieModel* m, float animStep, float run, float age, float headYaw, float headPitch);

#endif