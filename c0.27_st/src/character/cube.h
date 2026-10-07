// character/cube.h

#ifndef CUBE_H
#define CUBE_H

#include "polygon.h"

typedef struct {
    Quad polys[6];
    float   x, y, z;
    float   xRot, yRot, zRot;
    int     texOffX, texOffY;

    // cached geometry compiled into a display list on first render
    unsigned int displayList;
    int          built;
    int          visible;
    int          mirror;
} Cube;

void  Cube_init(Cube* c, int texOffX, int texOffY);
Cube* Cube_addBox(Cube* c, float ox, float oy, float oz, int w, int h, int d);
// expands the box on every side without changing its texture layout
Cube* Cube_addBoxGrown(Cube* c, float ox, float oy, float oz, int w, int h, int d, float grow);
void  Cube_setPos(Cube* c, float x, float y, float z);
void  Cube_render(Cube* c);

#endif
