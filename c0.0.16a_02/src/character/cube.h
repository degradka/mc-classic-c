// character/cube.h

#ifndef CUBE_H
#define CUBE_H

#include "polygon.h"

typedef struct {
    Polygon polys[6];
    float   x, y, z;
    float   xRot, yRot, zRot;
    int     texOffX, texOffY;

    // c0.0.14a_08: geometry is baked into a GL display list on first render
    // instead of re-issuing immediate mode vertices every frame
    unsigned int displayList;
    int          built;
} Cube;

void  Cube_init(Cube* c, int texOffX, int texOffY);
Cube* Cube_addBox(Cube* c, float ox, float oy, float oz, int w, int h, int d);
// c0.0.14a_08: hardcodes z to 0 regardless of the argument passed. Harmless
// since every call site (ZombieModel) always passes z=0.
void  Cube_setPos(Cube* c, float x, float y, float z);
void  Cube_render(Cube* c);

#endif
