// renderer/tessellator.h — client-side batched immediate-mode for quads

#ifndef TESSELLATOR_H
#define TESSELLATOR_H

#include <GL/glew.h>
#include <stdbool.h>

#define MAX_VERTICES 100000

typedef struct {
    float vertexBuffer[MAX_VERTICES * 3];
    float texBuffer   [MAX_VERTICES * 2];
    float colorBuffer [MAX_VERTICES * 3];

    int   vertices;

    // current attribs
    int   hasTexture; float u, v;
    int   hasColor;   float r, g, b;
    int   ignoreColor;
} Tessellator;

void Tessellator_begin         (Tessellator* t);
void Tessellator_vertex        (Tessellator* t, float x, float y, float z);
void Tessellator_texture       (Tessellator* t, float u, float v);
void Tessellator_vertexUV      (Tessellator* t, float x, float y, float z, float u, float v);
void Tessellator_color         (Tessellator* t, float r, float g, float b);
void Tessellator_setIgnoreColor(Tessellator* t, int ignore);
void Tessellator_end           (Tessellator* t);
void Tessellator_clear         (Tessellator* t);

static inline void Tessellator_colorBytes(Tessellator* t, unsigned char r, unsigned char g, unsigned char b) {
    Tessellator_color(t, r/255.0f, g/255.0f, b/255.0f);
}

static inline void Tessellator_colorInt(Tessellator* t, int c) {
    unsigned char r = (c >> 16) & 0xFF;
    unsigned char g = (c >>  8) & 0xFF;
    unsigned char b = (c      ) & 0xFF;
    Tessellator_color(t, r/255.0f, g/255.0f, b/255.0f);
}

#endif  // TESSELLATOR_H