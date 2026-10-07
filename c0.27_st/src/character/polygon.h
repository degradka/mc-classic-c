// character/polygon.h

#ifndef POLYGON_H
#define POLYGON_H

#include "vertex.h"

// not Polygon, that clashes with the windows gdi function
typedef struct { Vertex v[4]; } Quad;

void Quad_init_uv(Quad* p, Vertex a, Vertex b, Vertex c, Vertex d,
                     int minU, int minV, int maxU, int maxV);
void Quad_render(const Quad* p);

#endif
