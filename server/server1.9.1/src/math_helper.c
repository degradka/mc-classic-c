// math_helper.c

#include "math_helper.h"
#include <math.h>
#include <limits.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float SIN_TABLE[65536];
static int initialized = 0;

void MathHelper_init(void) {
    if (initialized) return;
    for (int i = 0; i < 65536; ++i) SIN_TABLE[i] = (float)sin((double)i * M_PI * 2.0 / 65536.0);
    // mingw's sin(pi) is off, it should be 1.2246467991473532E-16
    SIN_TABLE[32768] = (float)1.2246467991473532E-16;
    initialized = 1;
}

int MathHelper_floatToInt(float x) {
    if (x != x) return 0;
    if (x >= 2147483647.0f) return INT_MAX;
    if (x <= -2147483648.0f) return INT_MIN;
    return (int)x;
}

float MathHelper_sin(float x) {
    return SIN_TABLE[MathHelper_floatToInt(x * 10430.378f) & 0xFFFF];
}

float MathHelper_cos(float x) {
    return SIN_TABLE[MathHelper_floatToInt(x * 10430.378f + 16384.0f) & 0xFFFF];
}

float MathHelper_sqrt(float x) {
    return (float)sqrt((double)x);
}
