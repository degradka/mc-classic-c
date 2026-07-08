// gui/font.h

#ifndef FONT_H
#define FONT_H

#include <GL/glew.h>

#include "../renderer/tessellator.h"

typedef struct {
    int    charWidth[256];
    GLuint texture;
} Font;

void Font_init(Font* f, const char* path);                 // e.g. "resources/default.png"
void Font_draw(Font* f, Tessellator* t, const char* s, int x, int y, int color);
void Font_drawShadow(Font* f, Tessellator* t, const char* s, int x, int y, int color);
int  Font_width(Font* f, const char* s);

#endif // FONT_H
