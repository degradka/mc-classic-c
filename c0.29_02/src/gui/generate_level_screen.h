// gui/generate_level_screen.h: Small/Normal/Huge level size picker

#ifndef GENERATE_LEVEL_SCREEN_H
#define GENERATE_LEVEL_SCREEN_H

#include "screen.h"
#include "button.h"

typedef struct {
    Screen screen;
    Button buttons[4];
    Screen* previousScreen; // returned to on Cancel
} GenerateNewLevelScreen;

void GenerateNewLevelScreen_open(Screen* previousScreen);

#endif
