#ifndef FRUSTUM_H
#define FRUSTUM_H

#include "../phys/aabb.h"

typedef struct {
    float planes[6][4];
} Frustum;

void frustum_calculate(Frustum* f);
int  frustum_isVisible(const Frustum* f, const AABB* aabb);
int  frustum_cubeIn(const Frustum* f, float x1, float y1, float z1, float x2, float y2, float z2);
// every corner on the inner side of every plane
int  frustum_cubeFullyIn(const Frustum* f, float x1, float y1, float z1, float x2, float y2, float z2);

#endif