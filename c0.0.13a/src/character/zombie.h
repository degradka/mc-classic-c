// character/zombie.h

#ifndef ZOMBIE_H
#define ZOMBIE_H

#include "../entity.h"
#include "cube.h"
#include "zombie_model.h"

typedef struct {
    Entity base;
    ZombieModel model;
    float rotation;
    float rotationMotionFactor;
    float timeOffset;
    float speed;
} Zombie;

void Zombie_init  (Zombie* z, Level* level, float x, float y, float zpos);
void Zombie_onTick(Zombie* z);
void Zombie_render(const Zombie* z, float partialTicks);

#endif
