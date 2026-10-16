// gui/screen.h: screen base, fill, gradient, text and button helpers

#ifndef SCREEN_H
#define SCREEN_H

#include "font.h"
#include "button.h"
#include <stdbool.h>

typedef struct Screen Screen;

struct Screen {
    int width, height;
    Font* font;
    // Screen.allowUserInput, the player keeps moving and switching blocks while it's open
    bool allowUserInput;

    void (*render)(Screen* self, int xMouse, int yMouse);
    void (*init)(Screen* self);
    void (*tick)(Screen* self);
    void (*keyPressed)(Screen* self, char eventCharacter, int eventKey);
    void (*mouseClicked)(Screen* self, int x, int y, int button);
    void (*destroy)(Screen* self); // frees heap data owned by the screen, may be NULL
};

// Escape closes the screen and grabs the mouse
void Screen_defaultKeyPressed(Screen* self, char eventCharacter, int eventKey);

// colors are 0xAARRGGBB
void Screen_fill(int x0, int y0, int x1, int y1, unsigned int col);
void Screen_fillGradient(int x0, int y0, int x1, int y1, unsigned int col1, unsigned int col2);
void Screen_drawCenteredString(Screen* s, const char* str, int x, int y, unsigned int color);
void Screen_drawString(Screen* s, const char* str, int x, int y, unsigned int color);

void Screen_renderButtons(Screen* self, Button* buttons, int count, int xMouse, int yMouse);
// returns the clicked button's id, or -1 if the click didn't land on one
int  Screen_buttonClickedAt(Button* buttons, int count, int x, int y);

#endif
