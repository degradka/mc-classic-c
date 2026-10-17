// gui/controls_screen.h: key binding screen

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

void ControlsScreen_open(Screen* previousScreen, Options* options);

#endif
