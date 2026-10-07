// character/humanoid_armor_model.c

#include "humanoid_armor_model.h"

void HumanoidArmorModel_init(HumanoidArmorModel* m) {
    const float grow = 1.0f;

    Cube_init(&m->head, 0, 0);
    Cube_addBoxGrown(&m->head, -4, -8, -4, 8, 8, 8, grow);

    Cube_init(&m->body, 16, 16);
    Cube_addBoxGrown(&m->body, -4, 0, -2, 8, 12, 4, grow);

    Cube_init(&m->rightArm, 40, 16);
    Cube_addBoxGrown(&m->rightArm, -3, -2, -2, 4, 12, 4, grow);
    Cube_setPos(&m->rightArm, -5, 2, 0);

    Cube_init(&m->leftArm, 40, 16);
    m->leftArm.mirror = 1;
    Cube_addBoxGrown(&m->leftArm, -1, -2, -2, 4, 12, 4, grow);
    Cube_setPos(&m->leftArm, 5, 2, 0);
}

void HumanoidArmorModel_render(HumanoidArmorModel* m, bool helmet, bool armor,
                               const Cube* head, const Cube* rightArm, const Cube* leftArm) {
    m->head.visible = helmet;
    m->body.visible = armor;
    m->rightArm.visible = armor;
    m->leftArm.visible = armor;

    m->head.yRot = head->yRot;
    m->head.xRot = head->xRot;
    m->rightArm.xRot = rightArm->xRot;
    m->rightArm.zRot = rightArm->zRot;
    m->leftArm.xRot = leftArm->xRot;
    m->leftArm.zRot = leftArm->zRot;

    Cube_render(&m->head);
    Cube_render(&m->body);
    Cube_render(&m->rightArm);
    Cube_render(&m->leftArm);
}
