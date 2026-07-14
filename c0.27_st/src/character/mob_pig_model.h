// mob_pig_model.h: Pig's model (c0.24_st_03), matches mob/d/k.java. A
// forward-mounted head box, a torso box authored upright then rotated flat,
// and 4 short legs in a diagonal quadruped gait (front-left+back-right
// together, front-right+back-left together)

#ifndef MOB_PIG_MODEL_H
#define MOB_PIG_MODEL_H

#include "cube.h"

typedef struct {
    Cube head;
    Cube body;
    Cube legFrontLeft, legFrontRight, legBackLeft, legBackRight;
} MobPigModel;

void MobPigModel_init(MobPigModel* m);
// no attack swing parameter: Pig is purely passive, its Ai never attacks
void MobPigModel_render(MobPigModel* m, float animStep, float run, float age,
                        float headYaw, float headPitch);

#endif
