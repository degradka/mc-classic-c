// item/primed_tnt.h: a placed, ticking TNT block that's about to explode,
// matches item/PrimedTnt.java (new this version). Spawned whenever the TNT
// tile (id 46) is removed, whether by mining it or by being caught in
// another explosion's blast (see Tnt_onRemoved in level/tile/tile.c); both
// paths go through the same Level_netSetTile onRemoved hook, so mining and
// chain-reaction ignition are the same code path, matching the real source

#ifndef PRIMED_TNT_H
#define PRIMED_TNT_H

#include "../entity.h"

typedef struct {
    Entity e;
    int fuse; // ticks until explosion, counts down from 40
} PrimedTnt;

void PrimedTnt_init(PrimedTnt* t, Level* level, float x, float y, float z);
void PrimedTnt_onTick(PrimedTnt* t);
void PrimedTnt_render(const PrimedTnt* t, float partialTicks);

#endif
