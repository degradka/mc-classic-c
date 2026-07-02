// level/levelgen/synth/rotate.c

#include "rotate.h"
#include <math.h>

static double Rotate_getValue(const Synth* self, double x, double y) {
    const Rotate* r = (const Rotate*)self;
    return r->source->getValue(r->source, x * r->cos + y * r->sin, y * r->cos - x * r->sin);
}

void Rotate_init(Rotate* r, const Synth* source, double angle) {
    r->synth.getValue = Rotate_getValue;
    r->source = source;
    r->sin = sin(angle);
    r->cos = cos(angle);
}
