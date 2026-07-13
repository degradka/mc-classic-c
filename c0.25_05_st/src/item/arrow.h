// item/arrow.h: the player's arrow, shot with Tab

#ifndef ARROW_H
#define ARROW_H

#include "../entity.h"

typedef struct {
    Entity e;
    bool hasHit;
    int stickTime;
    int time;
    Entity* owner;
} Arrow;

void Arrow_init(Arrow* a, Level* level, Entity* owner, float x, float y, float z, float yaw, float pitch);
void Arrow_onTick(Arrow* a);
void Arrow_render(const Arrow* a, float partialTicks);

#endif
