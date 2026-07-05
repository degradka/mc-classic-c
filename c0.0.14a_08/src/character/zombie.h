// character/zombie.h

#ifndef ZOMBIE_H
#define ZOMBIE_H

#include "../entity.h"

typedef struct {
    Entity base;

    float  rotation;
    float  rotationMotionFactor;
    float  timeOffset;
    float  speed;
} Zombie;

void Zombie_init  (Zombie* z, Level* level, float x, float y, float zpos);
void Zombie_onTick(Zombie* z);
void Zombie_render(const Zombie* z, float partialTicks);

#endif