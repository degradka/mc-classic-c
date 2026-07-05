// level/levelgen/synth/improved_noise.h: Ken Perlin's improved noise

#ifndef IMPROVED_NOISE_H
#define IMPROVED_NOISE_H

#include "synth.h"
#include "../../../jrandom.h"

typedef struct {
    Synth synth;
    int p[512];
} ImprovedNoise;

void   ImprovedNoise_init(ImprovedNoise* n, JRandom* random);
double ImprovedNoise_noise(const ImprovedNoise* n, double x, double y, double z);

#endif
