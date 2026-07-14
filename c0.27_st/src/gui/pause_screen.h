// pause_screen.h: ESC menu, generate new level, save, load, back to game

#ifndef PAUSE_SCREEN_H
#define PAUSE_SCREEN_H

#include "screen.h"
#include "button.h"
#include "../options.h"

typedef struct {
    Screen screen; // first member: a PauseScreen* can be passed where a Screen* is expected
    Button buttons[5];
    Options* options; // c0.0.23a_01: passed through to the new Options... button
} PauseScreen;

void PauseScreen_init(PauseScreen* ps, Font* font, int width, int height, Options* options);

#endif
