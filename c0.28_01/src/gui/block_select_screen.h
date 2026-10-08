// gui/block_select_screen.h: the block picker on the build key

#ifndef BLOCK_SELECT_SCREEN_H
#define BLOCK_SELECT_SCREEN_H

#include "screen.h"
#include "../inventory.h"

typedef struct {
    Screen screen;
    Inventory* inventory;
    int terrainTex;
} BlockSelectScreen;

void BlockSelectScreen_open(Font* font, int width, int height, Inventory* inventory, int terrainTex);

#endif
