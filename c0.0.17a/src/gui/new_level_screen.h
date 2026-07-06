// gui/new_level_screen.h: picks the size of a new level

#ifndef NEW_LEVEL_SCREEN_H
#define NEW_LEVEL_SCREEN_H

#include "screen.h"

typedef struct {
    Screen screen; // must be first so NewLevelScreen* can be used as Screen*
} NewLevelScreen;

void NewLevelScreen_init(NewLevelScreen* s, Font* font, int width, int height);

#endif
