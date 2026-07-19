// mob_spider_model.h: Spider's model (c0.27_st, brand new this version),
// matches d/j.java exactly. 11 parts: head, a thorax that never moves off
// its own local origin, an abdomen, and 8 legs in 4 front-to-back pairs,
// each pair sharing its own independent walk-cycle phase (not a simple
// left/right alternation the way Pig's 4 legs are)

#ifndef MOB_SPIDER_MODEL_H
#define MOB_SPIDER_MODEL_H

#include "cube.h"

typedef struct {
    Cube head;
    Cube thorax;
    Cube abdomen;
    // front to back, matching d/j.java's own e/f (front), g/h, i/j, k/l (back)
    Cube legFrontLeft, legFrontRight;
    Cube legMidFrontLeft, legMidFrontRight;
    Cube legMidBackLeft, legMidBackRight;
    Cube legBackLeft, legBackRight;
} MobSpiderModel;

void MobSpiderModel_init(MobSpiderModel* m);
// no attackSwing parameter: matches every other quadruped model in this
// port, Spider has no arms to swing
void MobSpiderModel_render(MobSpiderModel* m, float animStep, float run, float age,
                           float headYaw, float headPitch);

#endif
