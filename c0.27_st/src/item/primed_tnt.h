// item/primed_tnt.h: lit TNT, explodes when its fuse runs out

#ifndef PRIMED_TNT_H
#define PRIMED_TNT_H

#include "../entity.h"

typedef struct {
    Entity e;
    int fuse; // ticks left, from 40
} PrimedTnt;

void PrimedTnt_init(PrimedTnt* t, Level* level, float x, float y, float z);
void PrimedTnt_onTick(PrimedTnt* t);
void PrimedTnt_render(const PrimedTnt* t, float partialTicks);

#endif
