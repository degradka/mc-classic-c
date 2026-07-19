// options_screen.h: c0.0.23a_01 Options menu, opened from the pause menu's
// new "Options..." button. Lists the 5 boolean/enum toggles plus a button
// to open the Controls (key remap) screen.

#ifndef OPTIONS_SCREEN_H
#define OPTIONS_SCREEN_H

#include "screen.h"
#include "button.h"
#include "../options.h"

// c0.27_st: music/sound/invertMouse/showFPS/viewDistance/bobView/anaglyph3d/
// limitFramerate, laid out as a 2 column grid matching c/e.java's own
// layout exactly (Options.s is simply this same count, read by the real
// source's own toggle loop instead of a hardcoded 7)
#define OPTIONS_TOGGLE_COUNT 8

typedef struct {
    Screen screen;
    Button buttons[OPTIONS_TOGGLE_COUNT + 2]; // toggles + "Controls..." + "Done"
    Screen* previousScreen;
    Options* options;
} OptionsScreen;

// initializes the single static instance and makes it the active screen
void OptionsScreen_open(Screen* previousScreen, Options* options);

#endif
