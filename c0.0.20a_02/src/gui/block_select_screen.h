// gui/block_select_screen.h: B shows every block, a click puts one in the selected slot

#ifndef BLOCK_SELECT_SCREEN_H
#define BLOCK_SELECT_SCREEN_H

#include "screen.h"

typedef struct {
    Screen screen; // must be first so BlockSelectScreen* can be used as Screen*
} BlockSelectScreen;

void BlockSelectScreen_init(BlockSelectScreen* s, Font* font, int width, int height);

#endif
