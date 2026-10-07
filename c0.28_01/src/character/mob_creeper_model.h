// character/mob_creeper_model.h

#ifndef MOB_CREEPER_MODEL_H
#define MOB_CREEPER_MODEL_H

#include "cube.h"

typedef struct {
    Cube head;
    Cube body;
    Cube legFrontLeft, legFrontRight, legBackLeft, legBackRight;
} MobCreeperModel;

void MobCreeperModel_init(MobCreeperModel* m);
void MobCreeperModel_render(MobCreeperModel* m, float animStep, float run, float age,
                            float headYaw, float headPitch);

#endif
