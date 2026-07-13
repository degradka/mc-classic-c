// jrandom.h: a 48 bit linear congruential random, the same seed gives the same rolls

#ifndef JRANDOM_H
#define JRANDOM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t seed;
    double nextNextGaussian;
    bool haveNextNextGaussian;
} JRandom;

void JRandom_setSeed(JRandom* r, int64_t seed);
// like new Random(), seeded from the clock and a counter so two calls never match
void JRandom_initRandom(JRandom* r);
int JRandom_nextInt(JRandom* r);
int JRandom_nextIntN(JRandom* r, int n);
float JRandom_nextFloat(JRandom* r);
double JRandom_nextDouble(JRandom* r);
double JRandom_nextGaussian(JRandom* r);

// Math.random(), one shared generator
double JRandom_mathRandom(void);

#endif
