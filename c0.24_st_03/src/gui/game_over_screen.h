// game_over_screen.h: shown once the player's health reaches 0 (permadeath).
// Only exit is "Generate new level...", matching the real source having no
// respawn option in this version (c0.24_st_03)

#ifndef GAME_OVER_SCREEN_H
#define GAME_OVER_SCREEN_H

#include "screen.h"
#include "button.h"

typedef struct {
    Screen screen;
    Button buttons[1];
    int score;
} GameOverScreen;

void GameOverScreen_init(GameOverScreen* s, Font* font, int width, int height, int score);

#endif
