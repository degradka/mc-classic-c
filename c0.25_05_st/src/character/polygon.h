// character/polygon.h

#ifndef POLYGON_H
#define POLYGON_H

#include "vertex.h"

// c0.25_05_st: renamed from Polygon, which collides with wingdi.h's own
// Polygon(HDC,...) GDI function once any file that includes windows.h
// (transitively, via net/net_socket.h) also pulls this header in
typedef struct { Vertex v[4]; } Quad;

void Quad_init_uv(Quad* p, Vertex a, Vertex b, Vertex c, Vertex d,
                     int minU, int minV, int maxU, int maxV);
void Quad_render(const Quad* p);

#endif
