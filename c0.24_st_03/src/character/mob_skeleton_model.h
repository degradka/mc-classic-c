// mob_skeleton_model.h: Skeleton's model (c0.24_st_03). Real source's
// Skeleton extends Zombie directly and only swaps in thinner arm/leg boxes
// (mob/d/f.java extends mob/d/e.java, overriding geometry only), same
// animation as MobZombieModel (arms locked forward, punch swing on attack)

#ifndef MOB_SKELETON_MODEL_H
#define MOB_SKELETON_MODEL_H

#include "cube.h"

typedef struct {
    Cube head;
    Cube body;
    Cube rightArm, leftArm;
    Cube rightLeg, leftLeg;
} MobSkeletonModel;

void MobSkeletonModel_init(MobSkeletonModel* m);
void MobSkeletonModel_render(MobSkeletonModel* m, float animStep, float run, float age,
                             float headYaw, float headPitch, float attackSwing);

#endif
