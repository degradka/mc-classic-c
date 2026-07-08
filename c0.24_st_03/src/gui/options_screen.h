// options_screen.h: c0.0.23a_01 Options menu, opened from the pause menu's
// new "Options..." button. Lists the 5 boolean/enum toggles plus a button
// to open the Controls (key remap) screen.

#ifndef OPTIONS_SCREEN_H
#define OPTIONS_SCREEN_H

#include "screen.h"
#include "button.h"
#include "../options.h"

typedef struct {
    Screen screen;
    Button buttons[7]; // 5 toggles + "Controls..." + "Done"
    Screen* previousScreen;
    Options* options;
} OptionsScreen;

// initializes the single static instance and makes it the active screen
void OptionsScreen_open(Screen* previousScreen, Options* options);

#endif
