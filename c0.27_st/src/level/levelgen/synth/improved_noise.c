// level/levelgen/synth/improved_noise.c

#include "improved_noise.h"
#include <math.h>
#include <stdlib.h>

static double fade(double t) {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

static double lerp(double t, double a, double b) {
    return a + t * (b - a);
}

static double grad(int hash, double x, double y, double z) {
    int h = hash & 0xF;
    double u = (h < 8) ? x : y;
    double v = (h < 4) ? y : ((h == 12 || h == 14) ? x : z);
    return (((h & 0x1) == 0) ? u : -u) + (((h & 0x2) == 0) ? v : -v);
}

double ImprovedNoise_noise(const ImprovedNoise* n, double x, double y, double z) {
    int X = (int)floor(x) & 0xFF;
    int Y = (int)floor(y) & 0xFF;
    int Z = (int)floor(z) & 0xFF;

    x -= floor(x);
    y -= floor(y);
    z -= floor(z);

    double u = fade(x);
    double v = fade(y);
    double w = fade(z);

    int A = n->p[X] + Y, AA = n->p[A] + Z, AB = n->p[A + 1] + Z;
    int B = n->p[X + 1] + Y, BA = n->p[B] + Z, BB = n->p[B + 1] + Z;

    return lerp(w,
        lerp(v, lerp(u, grad(n->p[AA], x, y, z), grad(n->p[BA], x - 1.0, y, z)),
                lerp(u, grad(n->p[AB], x, y - 1.0, z), grad(n->p[BB], x - 1.0, y - 1.0, z))),
        lerp(v, lerp(u, grad(n->p[AA + 1], x, y, z - 1.0), grad(n->p[BA + 1], x - 1.0, y, z - 1.0)),
                lerp(u, grad(n->p[AB + 1], x, y - 1.0, z - 1.0), grad(n->p[BB + 1], x - 1.0, y - 1.0, z - 1.0))));
}

static double ImprovedNoise_getValue(const Synth* self, double x, double y) {
    const ImprovedNoise* n = (const ImprovedNoise*)self;
    return ImprovedNoise_noise(n, x, y, 0.0);
}

void ImprovedNoise_init(ImprovedNoise* n) {
    n->synth.getValue = ImprovedNoise_getValue;

    for (int i = 0; i < 256; ++i) n->p[i] = i;

    for (int i = 0; i < 256; ++i) {
        int j = i + rand() % (256 - i);
        int tmp = n->p[i];
        n->p[i] = n->p[j];
        n->p[j] = tmp;
        n->p[i + 256] = n->p[i];
    }
}
