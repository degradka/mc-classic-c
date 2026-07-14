// character/mob_skeleton_model.h

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
