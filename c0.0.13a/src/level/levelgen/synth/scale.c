// level/levelgen/synth/scale.c

#include "scale.h"

static double Scale_getValue(const Synth* self, double x, double y) {
    const Scale* s = (const Scale*)self;
    return s->source->getValue(s->source, x * s->xScale, y * s->yScale);
}

void Scale_init(Scale* s, const Synth* source, double xScale, double yScale) {
    s->synth.getValue = Scale_getValue;
    s->source = source;
    s->xScale = 1.0 / xScale;
    s->yScale = 1.0 / yScale;
}
