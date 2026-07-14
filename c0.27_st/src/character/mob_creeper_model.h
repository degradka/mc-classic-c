// mob_creeper_model.h: Creeper's model (c0.24_st_03), matches mob/d/d.java.
// A Humanoid-shaped head+body (no arms) on 4 short legs, same diagonal
// quadruped gait as MobPigModel. No attack swing parameter: Creeper's melee
// touch damage plays no arm animation (it has no arms), and its explosion
// is a separate effect entirely (task #59/combat), not a model pose

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
