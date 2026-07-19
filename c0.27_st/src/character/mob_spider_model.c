// mob_spider_model.c: Spider's model, see mob_spider_model.h. Every box
// origin/size, position, and rotation constant below is read directly from
// d/j.java, not re-derived

#include "mob_spider_model.h"
#include "../common.h"
#include <math.h>

void MobSpiderModel_init(MobSpiderModel* m) {
    Cube_init(&m->head, 32, 4);
    Cube_addBox(&m->head, -4, -4, -8, 8, 8, 8);
    Cube_setPos(&m->head, 0, 0, -3);

    // thorax has no setPos call in the real source either, so it renders at
    // the model's own local origin, unlike every other part here
    Cube_init(&m->thorax, 0, 0);
    Cube_addBox(&m->thorax, -3, -3, -3, 6, 6, 6);

    Cube_init(&m->abdomen, 0, 12);
    Cube_addBox(&m->abdomen, -5, -4, -6, 10, 8, 12);
    Cube_setPos(&m->abdomen, 0, 0, 9);

    // all 8 legs share the same box shape, mirrored by choosing which side
    // of x=0 the 16 unit long box extends from (left legs: -15..1, right
    // legs: -1..15), not a true mirror flag
    Cube_init(&m->legFrontLeft, 18, 0);
    Cube_addBox(&m->legFrontLeft, -15, -1, -1, 16, 2, 2);
    Cube_setPos(&m->legFrontLeft, -4, 0, 2);

    Cube_init(&m->legFrontRight, 18, 0);
    Cube_addBox(&m->legFrontRight, -1, -1, -1, 16, 2, 2);
    Cube_setPos(&m->legFrontRight, 4, 0, 2);

    Cube_init(&m->legMidFrontLeft, 18, 0);
    Cube_addBox(&m->legMidFrontLeft, -15, -1, -1, 16, 2, 2);
    Cube_setPos(&m->legMidFrontLeft, -4, 0, 1);

    Cube_init(&m->legMidFrontRight, 18, 0);
    Cube_addBox(&m->legMidFrontRight, -1, -1, -1, 16, 2, 2);
    Cube_setPos(&m->legMidFrontRight, 4, 0, 1);

    Cube_init(&m->legMidBackLeft, 18, 0);
    Cube_addBox(&m->legMidBackLeft, -15, -1, -1, 16, 2, 2);
    Cube_setPos(&m->legMidBackLeft, -4, 0, 0);

    Cube_init(&m->legMidBackRight, 18, 0);
    Cube_addBox(&m->legMidBackRight, -1, -1, -1, 16, 2, 2);
    Cube_setPos(&m->legMidBackRight, 4, 0, 0);

    Cube_init(&m->legBackLeft, 18, 0);
    Cube_addBox(&m->legBackLeft, -15, -1, -1, 16, 2, 2);
    Cube_setPos(&m->legBackLeft, -4, 0, -1);

    Cube_init(&m->legBackRight, 18, 0);
    Cube_addBox(&m->legBackRight, -1, -1, -1, 16, 2, 2);
    Cube_setPos(&m->legBackRight, 4, 0, -1);
}

void MobSpiderModel_render(MobSpiderModel* m, float animStep, float run, float age,
                           float headYaw, float headPitch) {
    (void)age; // no arms, no idle sway term

    m->head.yRot = headYaw   / 57.29578f;
    m->head.xRot = headPitch / 57.29578f;

    // static leg splay (zRot/roll): front and back pairs +-45 degrees, the
    // two middle pairs +-33.3 degrees (45 * 0.74)
    float splay = 0.7853982f;
    m->legFrontLeft.zRot     = -splay;
    m->legFrontRight.zRot    =  splay;
    m->legMidFrontLeft.zRot  = -splay * 0.74f;
    m->legMidFrontRight.zRot =  splay * 0.74f;
    m->legMidBackLeft.zRot   = -splay * 0.74f;
    m->legMidBackRight.zRot  =  splay * 0.74f;
    m->legBackLeft.zRot      = -splay;
    m->legBackRight.zRot     =  splay;

    // static leg yaw: front pair +-45, back pair the same magnitude but with
    // left/right swapped relative to front, middle pairs +-22.5 each with
    // the same left/right swap between the two middle pairs, matching the
    // real source's own sign pattern exactly, not simplified
    float yaw = 0.3926991f;
    m->legFrontLeft.yRot     =  yaw * 2.0f;
    m->legFrontRight.yRot    = -yaw * 2.0f;
    m->legMidFrontLeft.yRot  =  yaw;
    m->legMidFrontRight.yRot = -yaw;
    m->legMidBackLeft.yRot   = -yaw;
    m->legMidBackRight.yRot  =  yaw;
    m->legBackLeft.yRot      = -yaw * 2.0f;
    m->legBackRight.yRot     =  yaw * 2.0f;

    // walk cycle: each of the 4 pairs gets its own phase (0, pi, pi/2,
    // 3pi/2), layered onto both yaw (cosine, double frequency) and roll
    // (absolute sine, single frequency)
    double t = animStep * 0.6662;
    float yawFront    = -(float)cos(t * 2.0)                    * 0.4f * run;
    float yawMidFront = -(float)cos(t * 2.0 + M_PI)              * 0.4f * run;
    float yawMidBack  = -(float)cos(t * 2.0 + M_PI / 2.0)        * 0.4f * run;
    float yawBack     = -(float)cos(t * 2.0 + 3.0 * M_PI / 2.0)  * 0.4f * run;

    float rollFront    = fabsf((float)sin(t)                    * 0.4f) * run;
    float rollMidFront = fabsf((float)sin(t + M_PI)              * 0.4f) * run;
    float rollMidBack  = fabsf((float)sin(t + M_PI / 2.0)        * 0.4f) * run;
    float rollBack     = fabsf((float)sin(t + 3.0 * M_PI / 2.0)  * 0.4f) * run;

    m->legFrontLeft.yRot     += yawFront;    m->legFrontRight.yRot    -= yawFront;
    m->legMidFrontLeft.yRot  += yawMidFront; m->legMidFrontRight.yRot -= yawMidFront;
    m->legMidBackLeft.yRot   += yawMidBack;  m->legMidBackRight.yRot  -= yawMidBack;
    m->legBackLeft.yRot      += yawBack;     m->legBackRight.yRot     -= yawBack;

    m->legFrontLeft.zRot     += rollFront;    m->legFrontRight.zRot    -= rollFront;
    m->legMidFrontLeft.zRot  += rollMidFront; m->legMidFrontRight.zRot -= rollMidFront;
    m->legMidBackLeft.zRot   += rollMidBack;  m->legMidBackRight.zRot  -= rollMidBack;
    m->legBackLeft.zRot      += rollBack;     m->legBackRight.zRot     -= rollBack;

    Cube_render(&m->head);
    Cube_render(&m->thorax);
    Cube_render(&m->abdomen);
    Cube_render(&m->legFrontLeft);
    Cube_render(&m->legFrontRight);
    Cube_render(&m->legMidFrontLeft);
    Cube_render(&m->legMidFrontRight);
    Cube_render(&m->legMidBackLeft);
    Cube_render(&m->legMidBackRight);
    Cube_render(&m->legBackLeft);
    Cube_render(&m->legBackRight);
}
