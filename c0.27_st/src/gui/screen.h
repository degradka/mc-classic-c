// gui/screen.h: screen base, fill, gradient, blit, text and button helpers

#ifndef SCREEN_H
#define SCREEN_H

#include "font.h"
#include "button.h"

typedef struct Screen Screen;

struct Screen {
    int width, height;
    Font* font;
    // some lists are never emptied on init, so it can grow
    Button* buttons;
    int buttonCount, buttonCapacity;
    // the game still takes keys and the wheel while it's open
    bool allowUserInput;

    void (*render)(Screen* self, int xMouse, int yMouse);
    // the key is a key code, 0 for a character that came without one
    void (*keyPressed)(Screen* self, char eventCharacter, int eventKey);
    void (*mousePressed)(Screen* self, int x, int y, int button);
    void (*buttonClicked)(Screen* self, Button* button);
    // runs every time it becomes the current screen
    void (*init)(Screen* self);
    void (*tick)(Screen* self);
    void (*closeScreen)(Screen* self);
};

// no buttons and the base hooks
void Screen_construct(Screen* s);
// Screen.init(minecraft, width, height)
void Screen_setup(Screen* s, Font* font, int width, int height);
Button* Screen_addButton(Screen* s, int id, int x, int y, int w, const char* msg);
void Screen_clearButtons(Screen* s);
// the base render, draws the buttons
void Screen_render(Screen* s, int xMouse, int yMouse);
void Screen_mousePressed(Screen* s, int x, int y, int button);
// escape closes the screen and grabs the mouse
void Screen_keyPressed(Screen* s, char eventCharacter, int eventKey);

// colors are 0xAARRGGBB
void Screen_fill(int x0, int y0, int x1, int y1, unsigned int col);
void Screen_fillGradient(int x0, int y0, int x1, int y1, unsigned int col1, unsigned int col2);
void Screen_drawCenteredString(Font* font, const char* str, int x, int y, unsigned int color);
void Screen_drawString(Font* font, const char* str, int x, int y, unsigned int color);
// Gui.blit, a piece of the bound 256x256 texture
void Screen_blit(int x, int y, int u, int v, int w, int h, float zLevel);

#endif
