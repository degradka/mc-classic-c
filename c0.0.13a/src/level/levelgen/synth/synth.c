// level/levelgen/synth/synth.c

#include "synth.h"
#include <stdlib.h>

double* Synth_create(const Synth* s, int width, int height) {
    double* result = (double*)malloc((size_t)width * height * sizeof(double));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            result[x + y * width] = s->getValue(s, x, y);
        }
    }
    return result;
}
