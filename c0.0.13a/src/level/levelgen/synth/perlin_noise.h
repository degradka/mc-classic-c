// level/levelgen/synth/perlin_noise.h — sum of ImprovedNoise octaves

#ifndef PERLIN_NOISE_H
#define PERLIN_NOISE_H

#include "synth.h"
#include "improved_noise.h"

typedef struct {
    Synth synth;
    ImprovedNoise* noiseLevels; // heap array, size == levels
    int levels;
} PerlinNoise;

void PerlinNoise_init(PerlinNoise* n, int levels);
void PerlinNoise_destroy(PerlinNoise* n);

#endif
