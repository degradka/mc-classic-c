// item/sign.h: a sign entity, thrown in front of the player and settles like an item
// its 4 lines can't be edited in this version

#ifndef SIGN_H
#define SIGN_H

#include "../entity.h"
#include "../gui/font.h"

typedef struct {
    Entity e;
    float rot;
} Sign;

// yaw in degrees, where the player looks
void Sign_init(Sign* s, Level* level, float x, float y, float z, float yaw);
void Sign_onTick(Sign* s);
void Sign_render(const Sign* s, float partialTicks, Font* font);

#endif
