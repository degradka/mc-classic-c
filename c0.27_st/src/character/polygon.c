// character/polygon.c

#include "polygon.h"
#include <GL/glew.h>

void Quad_init_uv(Quad* p, Vertex a, Vertex b, Vertex c, Vertex d,
                     int minU, int minV, int maxU, int maxV) {
    const float iu = 0.0015625f, iv = 0.003125f;
    p->v[0] = Vertex_remap(a, (float)maxU / 64.0f - iu, (float)minV / 32.0f + iv);
    p->v[1] = Vertex_remap(b, (float)minU / 64.0f + iu, (float)minV / 32.0f + iv);
    p->v[2] = Vertex_remap(c, (float)minU / 64.0f + iu, (float)maxV / 32.0f - iv);
    p->v[3] = Vertex_remap(d, (float)maxU / 64.0f - iu, (float)maxV / 32.0f - iv);
}

void Quad_render(const Quad* p) {
    // no glColor here, it would get baked into the cube's display list
    // and override the brightness the caller sets

    Vec3 e1 = Vec3_normalize(Vec3_sub(p->v[1].pos, p->v[0].pos));
    Vec3 e2 = Vec3_normalize(Vec3_sub(p->v[1].pos, p->v[2].pos));
    Vec3 n  = Vec3_normalize(Vec3_cross(e1, e2));
    glNormal3f(n.x, n.y, n.z);

    for (int i = 0; i < 4; ++i) {
        glTexCoord2f(p->v[i].u, p->v[i].v);
        glVertex3f(p->v[i].pos.x, p->v[i].pos.y, p->v[i].pos.z);
    }
}
