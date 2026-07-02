// level/levelgen/synth/synth.h — noise-generator base interface (not wired into terrain gen yet)

#ifndef SYNTH_H
#define SYNTH_H

typedef struct Synth Synth;

struct Synth {
    double (*getValue)(const Synth* self, double x, double y);
};

// caller frees the returned array
double* Synth_create(const Synth* s, int width, int height);

#endif
