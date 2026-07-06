// level/levelgen/synth/distort.h: offsets one Synth's x by another

#ifndef DISTORT_H
#define DISTORT_H

#include "synth.h"

typedef struct {
    Synth synth;
    const Synth* source;
    const Synth* distort;
} Distort;

void Distort_init(Distort* d, const Synth* source, const Synth* distort);

#endif
