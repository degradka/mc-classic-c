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
// matches character.a.a(animStep, run, age, headYaw, headPitch), c0.0.19a_04:
// run scales leg/arm swing amplitude (Zombie always passes 1.0, always
// "running"; NetworkPlayer eases it toward 0 when its player stops moving),
// age drives a small idle arm sway independent of animStep (Zombie always
// passes 0, only NetworkPlayer uses it). Head faces headYaw/headPitch
// directly; Zombie always passes 0,0, its head never turns independently
void ZombieModel_render(ZombieModel* m, float animStep, float run, float age, float headYaw, float headPitch);

#endif