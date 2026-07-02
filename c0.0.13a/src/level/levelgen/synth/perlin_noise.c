// level/levelgen/synth/perlin_noise.c

#include "perlin_noise.h"
#include <stdlib.h>

static double PerlinNoise_getValue(const Synth* self, double x, double y) {
    const PerlinNoise* n = (const PerlinNoise*)self;
    double value = 0.0;
    double pow = 1.0;
    for (int i = 0; i < n->levels; ++i) {
        value += ImprovedNoise_noise(&n->noiseLevels[i], x / pow, y / pow, 0.0) * pow;
        pow *= 2.0;
    }
    return value;
}

void PerlinNoise_init(PerlinNoise* n, int levels) {
    n->synth.getValue = PerlinNoise_getValue;
    n->levels = levels;
    n->noiseLevels = (ImprovedNoise*)malloc((size_t)levels * sizeof(ImprovedNoise));
    for (int i = 0; i < levels; ++i) {
        ImprovedNoise_init(&n->noiseLevels[i]);
    }
}

void PerlinNoise_destroy(PerlinNoise* n) {
    free(n->noiseLevels);
    n->noiseLevels = NULL;
}
