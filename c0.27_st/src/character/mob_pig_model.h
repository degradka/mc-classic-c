// mob_pig_model.h: the pig, its body box is built upright then turned flat

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
