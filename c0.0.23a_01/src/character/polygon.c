// character/polygon.c

#include "polygon.h"
#include <GL/glew.h>
#include <math.h>

void Polygon_init_uv(Polygon* p, Vertex a, Vertex b, Vertex c, Vertex d,
                     int minU, int minV, int maxU, int maxV) {
    p->v[0] = Vertex_remap(a, maxU, minV);
    p->v[1] = Vertex_remap(b, minU, minV);
    p->v[2] = Vertex_remap(c, minU, maxV);
    p->v[3] = Vertex_remap(d, maxU, maxV);
}

static Vec3 normalize(Vec3 v) {
    float f = (float)sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return Vec3_make(v.x / f, v.y / f, v.z / f);
}

static Vec3 sub(Vec3 a, Vec3 b) {
    return Vec3_make(a.x - b.x, a.y - b.y, a.z - b.z);
}

void Polygon_render(const Polygon* p, float scale) {
    Vec3 a = normalize(sub(p->v[1].pos, p->v[0].pos));
    Vec3 b = normalize(sub(p->v[1].pos, p->v[2].pos));
    Vec3 n = normalize(Vec3_make(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x));
    glNormal3f(n.x, n.y, n.z);
    const float uDiv = 64.0f;
    const float vDiv = 32.0f;

    // 19a goes through the corners the other way round, so the faces flip
    for (int i = 0; i < 4; i++) {
        glTexCoord2f(p->v[i].u / uDiv, p->v[i].v / vDiv);
        glVertex3f(p->v[i].pos.x * scale, p->v[i].pos.y * scale, p->v[i].pos.z * scale);
    }
}
