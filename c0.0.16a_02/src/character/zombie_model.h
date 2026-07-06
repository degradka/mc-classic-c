// zombie_model.h: extracted zombie model, cubes plus animation

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
// matches character.a.a(animStep, headYaw, headPitch): shared by Zombie
// (always passes 0,0 for head yaw/pitch, its head never turns independently)
// and NetworkPlayer (aims the head at wherever the remote player is looking)
void ZombieModel_render(ZombieModel* m, float animStep, float headYaw, float headPitch);

#endif