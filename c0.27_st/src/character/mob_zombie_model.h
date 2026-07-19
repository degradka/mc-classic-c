// character/mob_zombie_model.h: zombie mob model
// same boxes as zombie_model.h but the arms are held forward

#ifndef MOB_ZOMBIE_MODEL_H
#define MOB_ZOMBIE_MODEL_H

#include "cube.h"

typedef struct {
    Cube head;
    Cube body;
    Cube rightArm, leftArm;
    Cube rightLeg, leftLeg;
    // the hair layer, the head box grown by 0.5 at (32,0), follows the head
    Cube hair;
} MobZombieModel;

void MobZombieModel_init(MobZombieModel* m);
// attackSwing goes 0 to 1 over the punch
void MobZombieModel_render(MobZombieModel* m, float animStep, float run, float age,
                           float headYaw, float headPitch, float attackSwing);

#endif
