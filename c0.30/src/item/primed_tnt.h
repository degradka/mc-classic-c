// item/primed_tnt.h: lit TNT, explodes when its fuse runs out

#ifndef PRIMED_TNT_H
#define PRIMED_TNT_H

#include "../entity.h"

typedef struct {
    Entity e;
    float xd, yd, zd; // its own, Entity.move never zeroes them
    int life;
    // saved, nothing ever sets it
    bool defused;
} PrimedTnt;

void PrimedTnt_init(PrimedTnt* t, Level* level, float x, float y, float z);
void PrimedTnt_onTick(PrimedTnt* t);
// a player's hit puts it out, it drops as a tnt block
void PrimedTnt_hurt(PrimedTnt* t, Entity* attacker, bool byPlayer);
void PrimedTnt_render(const PrimedTnt* t, float partialTicks);

#endif
