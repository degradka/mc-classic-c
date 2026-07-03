// rotate.h: rotates the sampling coordinates of a source Synth

#ifndef ROTATE_H
#define ROTATE_H

#include "synth.h"

typedef struct {
    Synth synth;
    const Synth* source;
    double sin, cos;
} Rotate;

void Rotate_init(Rotate* r, const Synth* source, double angle);

#endif
