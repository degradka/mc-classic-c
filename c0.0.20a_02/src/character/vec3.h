// character/vec3.h

#ifndef VEC3_H
#define VEC3_H

#include <math.h>

typedef struct { float x, y, z; } Vec3;
static inline Vec3 Vec3_make(float x, float y, float z) { Vec3 v = { x, y, z }; return v; }
static inline Vec3 Vec3_lerp(Vec3 a, Vec3 b, float t) {
    return (Vec3){ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
}

// c0.0.20a_02: used to compute per face lighting normals
static inline Vec3 Vec3_sub(Vec3 a, Vec3 b) {
    return (Vec3){ a.x - b.x, a.y - b.y, a.z - b.z };
}
static inline Vec3 Vec3_normalize(Vec3 v) {
    float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    return (Vec3){ v.x / len, v.y / len, v.z / len };
}
static inline Vec3 Vec3_cross(Vec3 a, Vec3 b) {
    return (Vec3){ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

#endif
