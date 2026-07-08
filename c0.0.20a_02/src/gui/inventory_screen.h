// inventory_screen.h: c0.0.20a_02 "select block" screen, opened with B

#ifndef INVENTORY_SCREEN_H
#define INVENTORY_SCREEN_H

#include "screen.h"
#include "../level/level.h"

typedef struct {
    Screen screen; // first member: an InventoryScreen* can be passed where a Screen* is expected
    Level* level;
    int* hotbar;        // 9 element array, shared with minecraft.c's gHotbar
    int* selectedSlot;  // shared with minecraft.c's gSelectedSlot
    int textureId;      // terrain.png, for the isometric tile icons
} InventoryScreen;

void InventoryScreen_open(Font* font, int width, int height, Level* level, int* hotbar, int* selectedSlot, int textureId);

#endif
