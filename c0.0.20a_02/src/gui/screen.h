// screen.h: pause menu screen base, fill, gradient, text helpers, vtable

#ifndef SCREEN_H
#define SCREEN_H

#include "font.h"
#include "button.h"

typedef struct Screen Screen;

struct Screen {
    int width, height;
    Font* font;

    void (*render)(Screen* self, int xMouse, int yMouse);
    void (*init)(Screen* self);
    void (*tick)(Screen* self);
    void (*keyPressed)(Screen* self, char eventCharacter, int eventKey);
    void (*mouseClicked)(Screen* self, int x, int y, int button);
    void (*destroy)(Screen* self); // frees any heap data owned by a subtype; may be NULL
};

// default keyPressed: Escape closes the screen and grabs the mouse. Assign
// this to a concrete screen's keyPressed unless it needs its own handling.
void Screen_defaultKeyPressed(Screen* self, char eventCharacter, int eventKey);

// col/col1/col2 are 0xAARRGGBB (alpha in the high byte, matching the Java source)
void Screen_fill(int x0, int y0, int x1, int y1, unsigned int col);
void Screen_fillGradient(int x0, int y0, int x1, int y1, unsigned int col1, unsigned int col2);
void Screen_drawCenteredString(Screen* s, const char* str, int x, int y, unsigned int color);
void Screen_drawString(Screen* s, const char* str, int x, int y, unsigned int color);

// draws a button list the same way for every screen that has one: invisible
// buttons are skipped entirely, disabled ones get a bordered grey look,
// enabled ones get a black backing box plus hover/normal fill and text
void Screen_renderButtons(Screen* self, Button* buttons, int count, int xMouse, int yMouse);
// returns the clicked button's id, or -1 if the click didn't land on one
int  Screen_buttonClickedAt(Button* buttons, int count, int x, int y);

#endif
