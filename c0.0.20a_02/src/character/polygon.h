// character/polygon.h

#ifndef POLYGON_H
#define POLYGON_H

#include "vertex.h"

typedef struct { Vertex v[4]; } Polygon;

void Polygon_init_uv(Polygon* p, Vertex a, Vertex b, Vertex c, Vertex d,
                     int minU, int minV, int maxU, int maxV);
// the corners get scaled, the normal comes from the unscaled ones
void Polygon_render(const Polygon* p, float scale);

#endif
