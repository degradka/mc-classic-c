// game_over_screen.h: shown once the player's health reaches 0 (permadeath).
// No respawn option in this version, matching the real source; c0.25_05_st
// adds a second "Load level.." button, gated on a login session concept
// this port doesn't have, see game_over_screen.c

#ifndef GAME_OVER_SCREEN_H
#define GAME_OVER_SCREEN_H

#include "screen.h"
#include "button.h"

typedef struct {
    Screen screen;
    Button buttons[2];
    int score;
} GameOverScreen;

void GameOverScreen_init(GameOverScreen* s, Font* font, int width, int height, int score);

#endif
