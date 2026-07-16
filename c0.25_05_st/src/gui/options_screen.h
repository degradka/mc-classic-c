// options_screen.h: c0.0.23a_01 Options menu, opened from the pause menu's
// new "Options..." button. Lists the 5 boolean/enum toggles plus a button
// to open the Controls (key remap) screen.

#ifndef OPTIONS_SCREEN_H
#define OPTIONS_SCREEN_H

#include "screen.h"
#include "button.h"
#include "../options.h"

// c0.25_05_st: music/sound/invertMouse/showFPS/viewDistance/bobView/anaglyph3d,
// laid out as a 2 column grid matching c/e.java's own layout exactly
#define OPTIONS_TOGGLE_COUNT 7

typedef struct {
    Screen screen;
    Button buttons[OPTIONS_TOGGLE_COUNT + 2]; // toggles + "Controls..." + "Done"
    Screen* previousScreen;
    Options* options;
} OptionsScreen;

// initializes the single static instance and makes it the active screen
void OptionsScreen_open(Screen* previousScreen, Options* options);

#endif
