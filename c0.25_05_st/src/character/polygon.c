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
    glColor3f(1.f, 1.f, 1.f);

    // c0.0.20a_02: per face lighting normal, matching the real source's
    // normalize(v1-v0) cross normalize(v1-v2)
    Vec3 e1 = Vec3_normalize(Vec3_sub(p->v[1].pos, p->v[0].pos));
    Vec3 e2 = Vec3_normalize(Vec3_sub(p->v[1].pos, p->v[2].pos));
    Vec3 n  = Vec3_normalize(Vec3_cross(e1, e2));
    glNormal3f(n.x, n.y, n.z);

    // real source divides by the exact atlas size, 64.0/32.0; matching it now
    // instead of the anti-seam epsilon this port had been carrying
    const float uDiv = 64.0f;
    const float vDiv = 32.0f;

    glTexCoord2f(p->v[3].u / uDiv, p->v[3].v / vDiv); glVertex3f(p->v[3].pos.x, p->v[3].pos.y, p->v[3].pos.z);
    glTexCoord2f(p->v[2].u / uDiv, p->v[2].v / vDiv); glVertex3f(p->v[2].pos.x, p->v[2].pos.y, p->v[2].pos.z);
    glTexCoord2f(p->v[1].u / uDiv, p->v[1].v / vDiv); glVertex3f(p->v[1].pos.x, p->v[1].pos.y, p->v[1].pos.z);
    glTexCoord2f(p->v[0].u / uDiv, p->v[0].v / vDiv); glVertex3f(p->v[0].pos.x, p->v[0].pos.y, p->v[0].pos.z);
}
