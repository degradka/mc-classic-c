// item/sign.h: a free floating Sign entity (c0.24_st_03), matches
// item\Sign.java. Not a tile - tossed out in front of the player with a
// gentle physics arc and settles wherever it lands, same falls-and-bounces
// movement as Item. Text is 4 hardcoded lines, genuinely not editable in
// this build (confirmed: no text-entry path anywhere references it)

#ifndef SIGN_H
#define SIGN_H

#include "../entity.h"
#include "../gui/font.h"

typedef struct {
    Entity e;
    float rot; // board facing, independent of Entity's own (unused) yRotation
} Sign;

// yaw in degrees, matching the placing player's own current look direction
void Sign_init(Sign* s, Level* level, float x, float y, float z, float yaw);
void Sign_onTick(Sign* s);
void Sign_render(const Sign* s, float partialTicks, Font* font);

#endif
