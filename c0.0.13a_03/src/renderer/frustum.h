#ifndef FRUSTUM_H
#define FRUSTUM_H

#include "../phys/aabb.h"

typedef struct {
    float planes[6][4];
} Frustum;

void frustum_calculate(Frustum* f);
int  frustum_isVisible(const Frustum* f, const AABB* aabb);

#endif