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
    // c0.27_st: matches HumanoidMob's own always-on hair layer, see
    // mob_zombie_model.h's own identical field for the full explanation.
    // Skeleton's real geometry override only touches arms/legs, so this
    // uses the same head-sized box as Zombie's own hair
    Cube hair;
} MobSkeletonModel;

void MobSkeletonModel_init(MobSkeletonModel* m);
void MobSkeletonModel_render(MobSkeletonModel* m, float animStep, float run, float age,
                             float headYaw, float headPitch, float attackSwing);

#endif
