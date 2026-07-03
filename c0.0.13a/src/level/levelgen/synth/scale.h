// scale.h: scales the sampling coordinates of a source Synth

#ifndef SCALE_H
#define SCALE_H

#include "synth.h"

typedef struct {
    Synth synth;
    const Synth* source;
    double xScale, yScale; // stored as 1/scale, matching the Java source
} Scale;

void Scale_init(Scale* s, const Synth* source, double xScale, double yScale);

#endif
