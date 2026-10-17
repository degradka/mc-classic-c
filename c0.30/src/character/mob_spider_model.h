// character/mob_spider_model.h

#ifndef MOB_SPIDER_MODEL_H
#define MOB_SPIDER_MODEL_H

#include "cube.h"

typedef struct {
    Cube head;
    Cube thorax;
    Cube abdomen;
    // front to back
    Cube legFrontLeft, legFrontRight;
    Cube legMidFrontLeft, legMidFrontRight;
    Cube legMidBackLeft, legMidBackRight;
    Cube legBackLeft, legBackRight;
} MobSpiderModel;

void MobSpiderModel_init(MobSpiderModel* m);
void MobSpiderModel_render(MobSpiderModel* m, float animStep, float run, float age,
                           float headYaw, float headPitch);

#endif
