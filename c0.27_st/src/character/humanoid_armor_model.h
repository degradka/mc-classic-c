// character/humanoid_armor_model.h: armor overlay for the humanoid model

#ifndef HUMANOID_ARMOR_MODEL_H
#define HUMANOID_ARMOR_MODEL_H

#include <stdbool.h>
#include "cube.h"

typedef struct {
    Cube head;
    Cube body;
    Cube rightArm, leftArm;
} HumanoidArmorModel;

void HumanoidArmorModel_init(HumanoidArmorModel* m);

// head copies yaw and pitch, arms copy pitch and roll, the body never turns
void HumanoidArmorModel_render(HumanoidArmorModel* m, bool showHelmet, bool showArmor,
                               float headXRot, float headYRot,
                               float rightArmXRot, float rightArmZRot,
                               float leftArmXRot, float leftArmZRot);

#endif
