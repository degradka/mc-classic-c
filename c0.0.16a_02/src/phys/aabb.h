// aabb.h: axis aligned bounding boxes and collision helpers

#ifndef AABB_H
#define AABB_H

typedef struct {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
} AABB;

AABB  AABB_create(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);
AABB  AABB_clone(const AABB* a);
AABB  AABB_cloneMove(const AABB* a, float x, float y, float z);
AABB  AABB_expand(const AABB* a, float x, float y, float z);
AABB  AABB_grow(const AABB* a, float x, float y, float z);
float AABB_clipXCollide(const AABB* a, const AABB* b, float x);
float AABB_clipYCollide(const AABB* a, const AABB* b, float y);
float AABB_clipZCollide(const AABB* a, const AABB* b, float z);
int   AABB_intersects(const AABB* a, const AABB* b);
void  AABB_move(AABB* a, float x, float y, float z);
AABB  AABB_offset(const AABB* a, float x, float y, float z);

#endif // AABB_H
