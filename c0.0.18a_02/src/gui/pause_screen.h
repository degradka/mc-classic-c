// gui/pause_screen.h

#ifndef PAUSE_SCREEN_H
#define PAUSE_SCREEN_H

#include "screen.h"

typedef struct {
    Screen screen; // must be first so PauseScreen* can be used as Screen*
} PauseScreen;

void PauseScreen_init(PauseScreen* ps, Font* font, int width, int height);

#endif
