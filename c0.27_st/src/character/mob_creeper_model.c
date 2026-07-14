// mob_creeper_model.c: Creeper's model, see mob_creeper_model.h

#include "mob_creeper_model.h"
#include "../common.h"
#include <math.h>

void MobCreeperModel_init(MobCreeperModel* m) {
    Cube_init(&m->head, 0, 0);
    Cube_addBox(&m->head, -4, -8, -4, 8, 8, 8);

    Cube_init(&m->body, 16, 16);
    Cube_addBox(&m->body, -4, 0, -2, 8, 12, 4);

    Cube_init(&m->legFrontLeft, 0, 16);
    Cube_addBox(&m->legFrontLeft, -2, 0, -2, 4, 6, 4);
    Cube_setPos(&m->legFrontLeft, -2, 12, -4);

    Cube_init(&m->legFrontRight, 0, 16);
    Cube_addBox(&m->legFrontRight, -2, 0, -2, 4, 6, 4);
    Cube_setPos(&m->legFrontRight, 2, 12, -4);

    Cube_init(&m->legBackLeft, 0, 16);
    Cube_addBox(&m->legBackLeft, -2, 0, -2, 4, 6, 4);
    Cube_setPos(&m->legBackLeft, -2, 12, 4);

    Cube_init(&m->legBackRight, 0, 16);
    Cube_addBox(&m->legBackRight, -2, 0, -2, 4, 6, 4);
    Cube_setPos(&m->legBackRight, 2, 12, 4);
}

void MobCreeperModel_render(MobCreeperModel* m, float animStep, float run, float age,
                            float headYaw, float headPitch) {
    (void)age; // no arms, no idle sway term
    m->head.yRot = headYaw   / 57.29578f;
    m->head.xRot = headPitch / 57.29578f;

    m->legFrontRight.xRot = (float)cos(animStep * 0.6662) * 1.4f * run;
    m->legBackLeft.xRot   = (float)cos(animStep * 0.6662) * 1.4f * run;
    m->legFrontLeft.xRot  = (float)cos(animStep * 0.6662 + M_PI) * 1.4f * run;
    m->legBackRight.xRot  = (float)cos(animStep * 0.6662 + M_PI) * 1.4f * run;

    Cube_render(&m->head);
    Cube_render(&m->body);
    Cube_render(&m->legFrontLeft);
    Cube_render(&m->legFrontRight);
    Cube_render(&m->legBackLeft);
    Cube_render(&m->legBackRight);
}
