// distort.h: offsets a source Synth's X coordinate by another Synth

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
