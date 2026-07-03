// gui/pause_screen.h

#ifndef PAUSE_SCREEN_H
#define PAUSE_SCREEN_H

#include "screen.h"
#include "button.h"

typedef struct {
    Screen screen; // must be first so PauseScreen* can be used as Screen*
    Button buttons[4];
} PauseScreen;

void PauseScreen_init(PauseScreen* ps, Font* font, int width, int height);

#endif
