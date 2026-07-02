// level/levelgen/synth/improved_noise.h — Ken Perlin's "improved noise" (2002)

#ifndef IMPROVED_NOISE_H
#define IMPROVED_NOISE_H

#include "synth.h"

typedef struct {
    Synth synth;
    int p[512];
} ImprovedNoise;

void   ImprovedNoise_init(ImprovedNoise* n);
double ImprovedNoise_noise(const ImprovedNoise* n, double x, double y, double z);

#endif
