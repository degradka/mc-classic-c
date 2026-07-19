// character/humanoid_armor_model.c: see humanoid_armor_model.h

#include "humanoid_armor_model.h"

void HumanoidArmorModel_init(HumanoidArmorModel* m) {
    const float expand = 1.0f;

    Cube_init(&m->head, 0, 0);
    Cube_addBoxExpanded(&m->head, -4, -8, -4, 8, 8, 8, expand);

    Cube_init(&m->body, 16, 16);
    Cube_addBoxExpanded(&m->body, -4, 0, -2, 8, 12, 4, expand);

    Cube_init(&m->rightArm, 40, 16);
    Cube_addBoxExpanded(&m->rightArm, -3, -2, -2, 4, 12, 4, expand);
    Cube_setPos(&m->rightArm, -5, 2, 0);

    Cube_init(&m->leftArm, 40, 16);
    Cube_addBoxExpanded(&m->leftArm, -1, -2, -2, 4, 12, 4, expand);
    Cube_setPos(&m->leftArm, 5, 2, 0);
}

void HumanoidArmorModel_render(HumanoidArmorModel* m, bool showHelmet, bool showArmor,
                               float headXRot, float headYRot,
                               float rightArmXRot, float rightArmZRot,
                               float leftArmXRot, float leftArmZRot) {
    m->head.visible     = showHelmet;
    m->body.visible     = showArmor;
    m->rightArm.visible = showArmor;
    m->leftArm.visible  = showArmor;

    m->head.xRot = headXRot;
    m->head.yRot = headYRot;
    m->rightArm.xRot = rightArmXRot;
    m->rightArm.zRot = rightArmZRot;
    m->leftArm.xRot = leftArmXRot;
    m->leftArm.zRot = leftArmZRot;

    Cube_render(&m->head);
    Cube_render(&m->body);
    Cube_render(&m->rightArm);
    Cube_render(&m->leftArm);
}
