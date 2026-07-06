// pause_screen.h: ESC menu, generate new level, save, load, back to game

#ifndef PAUSE_SCREEN_H
#define PAUSE_SCREEN_H

#include "screen.h"
#include "button.h"

typedef struct {
    Screen screen; // first member: a PauseScreen* can be passed where a Screen* is expected
    Button buttons[4];
} PauseScreen;

void PauseScreen_init(PauseScreen* ps, Font* font, int width, int height);

#endif
