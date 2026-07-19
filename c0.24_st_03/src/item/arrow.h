// item/arrow.h: fired projectile entity (c0.24_st_03), matches item/Arrow.java.
// Singleplayer only in the real source (gated on no active connection, see
// the comment at its spawn site in minecraft.c). It is fully self contained
// and does its own small step incremental collision rather than using
// Entity_move/Mob_travel like everything else

#ifndef ARROW_H
#define ARROW_H

#include "../entity.h"

typedef struct {
    Entity e;
    bool hasHit;
    int stickTime;
    int time;
    Entity* owner; // the shooting player, for kill-score credit (Entity.killCredit)
} Arrow;

// yaw/pitch in degrees, matching the shooter's own current look direction
void Arrow_init(Arrow* a, Level* level, Entity* owner, float x, float y, float z, float yaw, float pitch);
void Arrow_onTick(Arrow* a);
void Arrow_render(const Arrow* a, float partialTicks);

#endif
