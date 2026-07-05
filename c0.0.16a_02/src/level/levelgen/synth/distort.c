// level/levelgen/synth/distort.c

#include "distort.h"

static double Distort_getValue(const Synth* self, double x, double y) {
    const Distort* d = (const Distort*)self;
    return d->source->getValue(d->source, x + d->distort->getValue(d->distort, x, y), y);
}

void Distort_init(Distort* d, const Synth* source, const Synth* distort) {
    d->synth.getValue = Distort_getValue;
    d->source = source;
    d->distort = distort;
}
