// gui/screen.h: screen base, fill, gradient, text and button helpers

#ifndef SCREEN_H
#define SCREEN_H

#include "font.h"

typedef struct Screen Screen;

struct Screen {
    int width, height;
    Font* font;

    void (*render)(Screen* self, int xMouse, int yMouse);
    void (*init)(Screen* self);
    void (*tick)(Screen* self);
    void (*keyPressed)(Screen* self, char eventCharacter, int eventKey);
    void (*mouseClicked)(Screen* self, int x, int y, int button);
    void (*destroy)(Screen* self); // frees heap data owned by the screen, may be NULL
};

// colors are 0xAARRGGBB
void Screen_fill(int x0, int y0, int x1, int y1, unsigned int col);
void Screen_fillGradient(int x0, int y0, int x1, int y1, unsigned int col1, unsigned int col2);
void Screen_drawCenteredString(Screen* s, const char* str, int x, int y, unsigned int color);
void Screen_drawString(Screen* s, const char* str, int x, int y, unsigned int color);

#endif
