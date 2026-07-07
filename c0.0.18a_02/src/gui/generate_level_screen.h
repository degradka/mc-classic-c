// generate_level_screen.h: Small/Normal/Huge/Cancel size picker, opened from
// the pause menu's "Generate new level..." button (c0.0.16a_02)

#ifndef GENERATE_LEVEL_SCREEN_H
#define GENERATE_LEVEL_SCREEN_H

#include "screen.h"
#include "button.h"

typedef struct {
    Screen screen;
    Button buttons[4];
    Screen* previousScreen; // returned to on Cancel
} GenerateNewLevelScreen;

// initializes the single static instance and makes it the active screen
void GenerateNewLevelScreen_open(Screen* previousScreen);

#endif
