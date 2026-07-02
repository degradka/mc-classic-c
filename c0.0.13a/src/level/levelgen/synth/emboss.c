// level/levelgen/synth/emboss.c

#include "emboss.h"

static double Emboss_getValue(const Synth* self, double x, double y) {
    const Emboss* e = (const Emboss*)self;
    return e->source->getValue(e->source, x, y) - e->source->getValue(e->source, x + 1.0, y + 1.0);
}

void Emboss_init(Emboss* e, const Synth* source) {
    e->synth.getValue = Emboss_getValue;
    e->source = source;
}
