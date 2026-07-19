// character/cube.h

#ifndef CUBE_H
#define CUBE_H

#include "polygon.h"

typedef struct {
    Quad polys[6];
    float   x, y, z;
    float   xRot, yRot, zRot;
    int     texOffX, texOffY;

    // c0.0.14a_08: geometry is baked into a GL display list on first render
    // instead of re-issuing immediate mode vertices every frame
    unsigned int displayList;
    int          built;

    // c0.27_st: matches real Cube's own "i" field, defaulting true; HumanoidMob's
    // armor overlay toggles this per part (helmet for head, armor for
    // body+arms, legs permanently false since Classic era plate armor never
    // covers them)
    int visible;
} Cube;

void  Cube_init(Cube* c, int texOffX, int texOffY);
Cube* Cube_addBox(Cube* c, float ox, float oy, float oz, int w, int h, int d);
// c0.27_st: matches real Cube's own box-builder taking an extra "expand"
// float, which inflates the box by that amount outward on every axis (UV atlas
// placement, driven by w/h/d, is unaffected). Used for HumanoidMob's hair
// layer (+0.5 over the head) and the whole "humanoid.armor" model (+1.0
// over every part), both built with real source's own single generic
// "h(float expand)" constructor; Cube_addBox is just this with expand=0
Cube* Cube_addBoxExpanded(Cube* c, float ox, float oy, float oz, int w, int h, int d, float expand);
void  Cube_setPos(Cube* c, float x, float y, float z);
void  Cube_render(Cube* c);

#endif
