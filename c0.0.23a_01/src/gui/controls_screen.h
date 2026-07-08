// controls_screen.h: c0.0.23a_01 key remap screen, opened from the Options
// screen's "Controls..." button. Lists all 10 bindings in a 2 column layout;
// clicking one enters "press a key" capture mode.

#ifndef CONTROLS_SCREEN_H
#define CONTROLS_SCREEN_H

#include "screen.h"
#include "button.h"
#include "../options.h"

typedef struct {
    Screen screen;
    Button buttons[OPTIONS_KEY_COUNT + 1]; // 10 bindings + "Done"
    Screen* previousScreen;
    Options* options;
    int capturingIndex; // -1 = not capturing, else index into options->keys[]
} ControlsScreen;

// initializes the single static instance and makes it the active screen
void ControlsScreen_open(Screen* previousScreen, Options* options);

#endif
