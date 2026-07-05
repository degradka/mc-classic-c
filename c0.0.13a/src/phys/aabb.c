// phys/aabb.c

#include "aabb.h"

AABB AABB_create(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
    AABB aabb;
    aabb.minX = minX; aabb.minY = minY; aabb.minZ = minZ;
    aabb.maxX = maxX; aabb.maxY = maxY; aabb.maxZ = maxZ;
    return aabb;
}

AABB AABB_clone(const AABB* a) {
    return AABB_create(a->minX, a->minY, a->minZ, a->maxX, a->maxY, a->maxZ);
}

// cloneMove uses z instead of x for minX
AABB AABB_cloneMove(const AABB* a, float x, float y, float z) {
    return AABB_create(a->minX + z, a->minY + y, a->minZ + z,
                       a->maxX + x, a->maxY + y, a->maxZ + z);
}

AABB AABB_expand(const AABB* a, float x, float y, float z) {
    float minX = a->minX, minY = a->minY, minZ = a->minZ;
    float maxX = a->maxX, maxY = a->maxY, maxZ = a->maxZ;

    if (x < 0.0f) minX += x;
    if (x > 0.0f) maxX += x;
    if (y < 0.0f) minY += y;
    if (y > 0.0f) maxY += y;
    if (z < 0.0f) minZ += z;
    if (z > 0.0f) maxZ += z;

    return AABB_create(minX, minY, minZ, maxX, maxY, maxZ);
}

AABB AABB_grow(const AABB* a, float x, float y, float z) {
    return AABB_create(a->minX - x, a->minY - y, a->minZ - z,
                       a->maxX + x, a->maxY + y, a->maxZ + z);
}

float AABB_clipXCollide(const AABB* a, const AABB* b, float x) {
    if (b->maxY <= a->minY || b->minY >= a->maxY) return x;
    if (b->maxZ <= a->minZ || b->minZ >= a->maxZ) return x;

    if (x > 0.0f && b->maxX <= a->minX) {
        float max = a->minX - b->maxX;
        if (max < x) x = max;
    }
    if (x < 0.0f && b->minX >= a->maxX) {
        float max = a->maxX - b->minX;
        if (max > x) x = max;
    }
    return x;
}

float AABB_clipYCollide(const AABB* a, const AABB* b, float y) {
    if (b->maxX <= a->minX || b->minX >= a->maxX) return y;
    if (b->maxZ <= a->minZ || b->minZ >= a->maxZ) return y;

    if (y > 0.0f && b->maxY <= a->minY) {
        float max = a->minY - b->maxY;
        if (max < y) y = max;
    }
    if (y < 0.0f && b->minY >= a->maxY) {
        float max = a->maxY - b->minY;
        if (max > y) y = max;
    }
    return y;
}

float AABB_clipZCollide(const AABB* a, const AABB* b, float z) {
    if (b->maxX <= a->minX || b->minX >= a->maxX) return z;
    if (b->maxY <= a->minY || b->minY >= a->maxY) return z;

    if (z > 0.0f && b->maxZ <= a->minZ) {
        float max = a->minZ - b->maxZ;
        if (max < z) z = max;
    }
    if (z < 0.0f && b->minZ >= a->maxZ) {
        float max = a->maxZ - b->minZ;
        if (max > z) z = max;
    }
    return z;
}

int AABB_intersects(const AABB* a, const AABB* b) {
    if (b->maxX <= a->minX || b->minX >= a->maxX) return 0;
    if (b->maxY <= a->minY || b->minY >= a->maxY) return 0;
    if (b->maxZ <= a->minZ || b->minZ >= a->maxZ) return 0;
    return 1;
}

void AABB_move(AABB* a, float x, float y, float z) {
    a->minX += x; a->minY += y; a->minZ += z;
    a->maxX += x; a->maxY += y; a->maxZ += z;
}
