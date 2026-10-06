// gui/game_over_screen.h: shown when health hits 0

#ifndef GAME_OVER_SCREEN_H
#define GAME_OVER_SCREEN_H

#include "screen.h"
#include "button.h"

typedef struct {
    Screen screen;
    Button buttons[2];
    const int* score; // read live
} GameOverScreen;

void GameOverScreen_init(GameOverScreen* s, Font* font, int width, int height, const int* score);

#endif
