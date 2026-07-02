// level/levelgen/synth/emboss.h — first-derivative-ish edge emphasis over a source Synth

#ifndef EMBOSS_H
#define EMBOSS_H

#include "synth.h"

typedef struct {
    Synth synth;
    const Synth* source;
} Emboss;

void Emboss_init(Emboss* e, const Synth* source);

#endif
