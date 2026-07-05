// gui/screen.h: screen base, fill, gradient, text and button helpers

#ifndef SCREEN_H
#define SCREEN_H

#include "font.h"
#include "button.h"

#define SCREEN_MAX_BUTTONS 8

typedef struct Screen Screen;

struct Screen {
    int width, height;
    Font* font;
    Button buttons[SCREEN_MAX_BUTTONS];
    int buttonCount;

    void (*render)(Screen* self, int xMouse, int yMouse);
    void (*keyPressed)(Screen* self, char eventCharacter, int eventKey);
    void (*buttonClicked)(Screen* self, Button* button);
    void (*tick)(Screen* self);
    // called when another screen replaces this one, may be NULL
    void (*removed)(Screen* self);
};

void Screen_initBase(Screen* s, Font* font, int width, int height);
void Screen_addButton(Screen* s, int id, int x, int y, int w, int h, const char* msg);
// the base render, draws the buttons
void Screen_render(Screen* s, int xMouse, int yMouse);
// escape closes the screen and grabs the mouse
void Screen_keyPressed(Screen* s, char eventCharacter, int eventKey);

// colors are 0xAARRGGBB
void Screen_fill(int x0, int y0, int x1, int y1, unsigned int col);
void Screen_fillGradient(int x0, int y0, int x1, int y1, unsigned int col1, unsigned int col2);
void Screen_drawCenteredString(Screen* s, const char* str, int x, int y, unsigned int color);
void Screen_drawString(Screen* s, const char* str, int x, int y, unsigned int color);

#endif
