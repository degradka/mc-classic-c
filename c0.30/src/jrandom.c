// jrandom.c

#include "jrandom.h"
#include <math.h>
#include <time.h>

#define MULTIPLIER 0x5DEECE66DULL
#define ADDEND 0xBULL
#define MASK ((1ULL << 48) - 1)

void JRandom_setSeed(JRandom* r, int64_t seed) {
    r->seed = ((uint64_t)seed ^ MULTIPLIER) & MASK;
    r->haveNextNextGaussian = false;
}

void JRandom_initRandom(JRandom* r) {
    static uint64_t uniquifier = 8682522807148012ULL;
    uniquifier *= 181783497276652981ULL;
    uint64_t now = (uint64_t)time(NULL) * 1000000000ULL + (uint64_t)clock();
    JRandom_setSeed(r, (int64_t)(uniquifier ^ now));
}

static int32_t next(JRandom* r, int bits) {
    r->seed = (r->seed * MULTIPLIER + ADDEND) & MASK;
    return (int32_t)(int64_t)(r->seed >> (48 - bits));
}

int JRandom_nextInt(JRandom* r) {
    return next(r, 32);
}

int JRandom_nextIntN(JRandom* r, int n) {
    if ((n & -n) == n) return (int)(((int64_t)n * (int64_t)next(r, 31)) >> 31);
    int32_t bits, val;
    do {
        bits = next(r, 31);
        val = bits % n;
    } while ((int32_t)((uint32_t)bits - (uint32_t)val + (uint32_t)(n - 1)) < 0);
    return val;
}

float JRandom_nextFloat(JRandom* r) {
    return (float)next(r, 24) / (float)(1 << 24);
}

double JRandom_nextDouble(JRandom* r) {
    return (double)(((int64_t)next(r, 26) << 27) + next(r, 27)) * (1.0 / (double)(1LL << 53));
}

double JRandom_nextGaussian(JRandom* r) {
    if (r->haveNextNextGaussian) {
        r->haveNextNextGaussian = false;
        return r->nextNextGaussian;
    }
    double v1, v2, s;
    do {
        v1 = 2.0 * JRandom_nextDouble(r) - 1.0;
        v2 = 2.0 * JRandom_nextDouble(r) - 1.0;
        s = v1 * v1 + v2 * v2;
    } while (s >= 1.0 || s == 0.0);
    double multiplier = sqrt(-2.0 * log(s) / s);
    r->nextNextGaussian = v2 * multiplier;
    r->haveNextNextGaussian = true;
    return v1 * multiplier;
}

double JRandom_mathRandom(void) {
    static JRandom shared;
    static bool seeded = false;
    if (!seeded) {
        JRandom_initRandom(&shared);
        seeded = true;
    }
    return JRandom_nextDouble(&shared);
}
