// gui/options_screen.h

#ifndef OPTIONS_SCREEN_H
#define OPTIONS_SCREEN_H

#include "screen.h"
#include "button.h"
#include "../options.h"

#define OPTIONS_TOGGLE_COUNT 7

typedef struct {
    Screen screen;
    Button buttons[OPTIONS_TOGGLE_COUNT + 2]; // toggles + "Controls..." + "Done"
    Screen* previousScreen;
    Options* options;
} OptionsScreen;

void OptionsScreen_open(Screen* previousScreen, Options* options);

#endif
