// character/polygon.c

#include "polygon.h"
#include <GL/glew.h>

void Polygon_init_uv(Polygon* p, Vertex a, Vertex b, Vertex c, Vertex d,
                     int minU, int minV, int maxU, int maxV) {
    p->v[0] = Vertex_remap(a, maxU, minV);
    p->v[1] = Vertex_remap(b, minU, minV);
    p->v[2] = Vertex_remap(c, minU, maxV);
    p->v[3] = Vertex_remap(d, maxU, maxV);
}

void Polygon_render(const Polygon* p) {
    const float uDiv = 64.0f;
    const float vDiv = 32.0f;

    // 19a goes through the corners the other way round, so the faces flip
    for (int i = 0; i < 4; i++) {
        glTexCoord2f(p->v[i].u / uDiv, p->v[i].v / vDiv);
        glVertex3f(p->v[i].pos.x, p->v[i].pos.y, p->v[i].pos.z);
    }
}
